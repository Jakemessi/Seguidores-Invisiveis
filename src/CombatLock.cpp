#include "PCH.h"
#include "CombatLock.h"
#include "Logger.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <thread>

// Reune as regras que impedem invocacoes aliadas de lutar fora do combate do jogador.
namespace IAFT
{
    namespace
    {
        // Define o intervalo das verificacoes e as flags usadas para bloquear acoes.
        constexpr auto kIntervaloMonitor = std::chrono::milliseconds(400);
        constexpr auto kAtaqueDesabilitado = RE::Actor::BOOL_FLAGS::kAttackingDisabled;
        constexpr auto kConjuracaoDesabilitada = RE::Actor::BOOL_FLAGS::kCastingDisabled;
        // Identifica os dados de Actor Values gravados no co-save do SKSE.
        constexpr std::uint32_t kIdentificadorSerializacao = 0x54464149; // IAFT
        constexpr std::uint32_t kTipoRegistroAV = 0x56414941;           // AIAV
        constexpr std::uint32_t kVersaoRegistroAV = 1;
        constexpr std::uint32_t kAVAlteradoAggression = 1;
        constexpr std::uint32_t kAVAlteradoAssistance = 2;

        // Guarda os valores anteriores para recupera-los apos carregar um save.
        struct RegistroAVPersistido
        {
            std::uint32_t formID = 0;
            float agressividadeOriginal = 0.0f;
            float assistenciaOriginal = 0.0f;
            std::uint32_t flags = 0;
        };

        // Registra o estado original de cada invocacao temporariamente bloqueada.
        struct Bloqueio
        {
            RE::ActorHandle handle;
            bool ataqueJaDesabilitado = false;
            bool conjuracaoJaDesabilitada = false;
            float agressividadeOriginal = 0.0f;
            float assistenciaOriginal = 0.0f;
            bool agressividadeAlterada = false;
            bool assistenciaAlterada = false;
        };

        // Mantem os bloqueios ativos, os valores pendentes e os inimigos da luta.
        std::vector<Bloqueio> g_bloqueios;
        std::vector<RegistroAVPersistido> g_avPendentes;
        std::vector<RE::ActorHandle> g_inimigosConfronto;
        // Guarda o filtro de invocacoes e informa se o combate esta liberado.
        std::function<bool(RE::Actor*)> g_eInvocacaoAliada;
        bool g_combateLiberado = false;

        // O monitor apenas agenda tarefas. Todos os acessos a atores/flags
        // acontecem nas tarefas da thread principal do Skyrim.
        std::atomic<std::uint64_t> g_geracao{ 0 };
        std::atomic<bool> g_tarefaPendente{ false };
        RE::ScriptEventSourceHolder* g_fonteEventosCombate = nullptr;

        void AgendarAtualizacao();

        // Recebe avisos do jogo quando o estado de combate de algum ator muda.
        class EventoCombate final : public RE::BSTEventSink<RE::TESCombatEvent>
        {
        public:
            RE::BSEventNotifyControl ProcessEvent(
                const RE::TESCombatEvent* evento,
                RE::BSTEventSource<RE::TESCombatEvent>*) override
            {
                // Um evento valido pede uma nova verificacao, sem agir aqui diretamente.
                if (evento) {
                    AgendarAtualizacao();
                }
                return RE::BSEventNotifyControl::kContinue;
            }
        };

        EventoCombate g_sinkCombate;
        // Declarado por ultimo para ser destruido primeiro no encerramento.
        std::jthread g_monitor;

        // Salva os valores antigos das invocacoes para restaura-los ao carregar o jogo.
        void SalvarAVs(SKSE::SerializationInterface* serializacao)
        {
            // Comeca com os valores ainda pendentes e acrescenta os bloqueios atuais.
            std::vector<RegistroAVPersistido> registros = g_avPendentes;
            for (const auto& bloqueio : g_bloqueios) {
                const auto ator = bloqueio.handle.get();
                if (!ator || (!bloqueio.agressividadeAlterada && !bloqueio.assistenciaAlterada)) {
                    continue;
                }

                // Copia a identificacao do ator, os valores originais e o que foi alterado.
                RegistroAVPersistido registro{
                    ator->GetFormID(),
                    bloqueio.agressividadeOriginal,
                    bloqueio.assistenciaOriginal,
                    (bloqueio.agressividadeAlterada ? kAVAlteradoAggression : 0u) |
                        (bloqueio.assistenciaAlterada ? kAVAlteradoAssistance : 0u)
                };
                if (registro.formID == 0) {
                    continue;
                }

                // Atualiza um registro existente para nao salvar o mesmo ator duas vezes.
                auto it = std::find_if(registros.begin(), registros.end(),
                    [&](const RegistroAVPersistido& atual) {
                        return atual.formID == registro.formID;
                    });
                if (it != registros.end()) {
                    *it = registro;
                } else {
                    registros.push_back(registro);
                }
            }

            // Sem registros ou sem espaco no co-save, nao ha nada para gravar.
            if (registros.empty() || !serializacao->OpenRecord(kTipoRegistroAV, kVersaoRegistroAV)) {
                return;
            }

            // Grava primeiro a quantidade e depois os dados de cada ator.
            const auto quantidade = static_cast<std::uint32_t>(registros.size());
            if (!serializacao->WriteRecordData(quantidade)) {
                return;
            }
            for (const auto& registro : registros) {
                if (!serializacao->WriteRecordData(registro)) {
                    LogErro("Erro ao serializar AVs das invocacoes",
                            "Could not serialize allied summon actor values");
                    break;
                }
            }
        }

        // Le do co-save os valores antigos que precisarao ser restaurados.
        void CarregarAVs(SKSE::SerializationInterface* serializacao)
        {
            // Limpa a lista anterior e prepara os dados de cada registro salvo.
            g_avPendentes.clear();
            std::uint32_t tipo = 0;
            std::uint32_t versao = 0;
            std::uint32_t tamanho = 0;
            while (serializacao->GetNextRecordInfo(tipo, versao, tamanho)) {
                // Ignora registros de outro tipo, versao ou tamanho invalido.
                if (tipo != kTipoRegistroAV || versao != kVersaoRegistroAV ||
                    tamanho < sizeof(std::uint32_t)) {
                    continue;
                }

                // Confere o tamanho e limita a quantidade para evitar dados corrompidos.
                std::uint32_t quantidade = 0;
                if (serializacao->ReadRecordData(quantidade) != sizeof(quantidade) ||
                    quantidade > 4096 ||
                    quantidade > (tamanho - sizeof(quantidade)) / sizeof(RegistroAVPersistido)) {
                    LogErro("Registro invalido de AVs no co-save",
                            "Invalid actor-value record in SKSE co-save");
                    continue;
                }

                // Le cada ator e converte seu FormID para o save atual.
                for (std::uint32_t i = 0; i < quantidade; ++i) {
                    RegistroAVPersistido registro;
                    if (serializacao->ReadRecordData(registro) != sizeof(registro)) {
                        break;
                    }
                    RE::FormID formIDAtual = 0;
                    if (serializacao->ResolveFormID(registro.formID, formIDAtual)) {
                        registro.formID = formIDAtual;
                        g_avPendentes.push_back(registro);
                    }
                }
            }
        }

        // Esquece restauracoes pendentes ao reverter o estado da serializacao.
        void ReverterAVs(SKSE::SerializationInterface*)
        {
            g_avPendentes.clear();
        }

        // Tenta restaurar valores lidos do co-save assim que os atores estiverem disponiveis.
        void RestaurarAVsPendentes()
        {
            for (auto it = g_avPendentes.begin(); it != g_avPendentes.end();) {
                // Procura o ator pelo identificador salvo; se nao carregou, tenta depois.
                auto* ator = RE::TESForm::LookupByID<RE::Actor>(it->formID);
                if (!ator) {
                    ++it; // Uma invocacao ainda pode nao ter carregado.
                    continue;
                }

                // Preserva o registro caso a interface ainda nao esteja disponivel.
                auto* valores = ator->AsActorValueOwner();
                if (!valores) {
                    ++it;
                    continue;
                }

                // Nunca sobrescreve a modificacao de outro mod.
                if ((it->flags & kAVAlteradoAggression) &&
                    valores->GetActorValue(RE::ActorValue::kAggression) == 0.0f) {
                    valores->SetActorValue(RE::ActorValue::kAggression,
                                          it->agressividadeOriginal);
                }

                if ((it->flags & kAVAlteradoAssistance) &&
                    valores->GetActorValue(RE::ActorValue::kAssistance) == 0.0f) {
                    valores->SetActorValue(RE::ActorValue::kAssistance,
                                          it->assistenciaOriginal);
                }
                // Remove apenas os registros que ja passaram pela tentativa de restauracao.
                it = g_avPendentes.erase(it);
            }
        }

        // Aceita somente atores vivos identificados como invocacoes aliadas.
        bool EInvocacaoAliada(RE::Actor* actor)
        {
            return actor && !actor->IsDead() &&
                   g_eInvocacaoAliada && g_eInvocacaoAliada(actor);
        }

        // Devolve as permissoes de ataque, conjuracao e os AVs alterados pelo plugin.
        void RestaurarAtaques(const Bloqueio& bloqueio)
        {
            auto ator = bloqueio.handle.get();
            if (!ator) {
                return;
            }

            // So reabilita acoes que nao estavam desabilitadas antes do bloqueio.
            auto& flags = ator->GetActorRuntimeData().boolFlags;
            if (!bloqueio.ataqueJaDesabilitado) {
                flags.reset(kAtaqueDesabilitado);
            }
            if (!bloqueio.conjuracaoJaDesabilitada) {
                flags.reset(kConjuracaoDesabilitada);
            }

            // Restaura apenas os AVs modificados pela DLL. Se outro mod
            // alterou o valor enquanto estava bloqueado, respeita a alteracao.
            if (auto* valores = ator->AsActorValueOwner()) {
                if (bloqueio.agressividadeAlterada &&
                    valores->GetActorValue(RE::ActorValue::kAggression) == 0.0f) {
                    valores->SetActorValue(RE::ActorValue::kAggression,
                                          bloqueio.agressividadeOriginal);
                }

                if (bloqueio.assistenciaAlterada &&
                    valores->GetActorValue(RE::ActorValue::kAssistance) == 0.0f) {
                    valores->SetActorValue(RE::ActorValue::kAssistance,
                                          bloqueio.assistenciaOriginal);
                }
            }
        }

        // Restaura cada invocacao bloqueada e esvazia a lista de bloqueios.
        void RestaurarTodos()
        {
            for (const auto& bloqueio : g_bloqueios) {
                RestaurarAtaques(bloqueio);
            }
            g_bloqueios.clear();
        }

        // Evita aplicar o mesmo bloqueio duas vezes no mesmo ator.
        bool JaBloqueado(const RE::ActorHandle& handle)
        {
            return std::any_of(g_bloqueios.begin(), g_bloqueios.end(),
                [&handle](const Bloqueio& bloqueio) {
                    return bloqueio.handle == handle;
                });
        }

        // Mantem bloqueadas apenas as invocacoes aliadas enquanto nao ha confronto.
        void SincronizarBloqueios()
        {
            // Se um morto reanimado perder o comandante ou morrer,
            // os flags que adicionamos sao restaurados.
            for (auto it = g_bloqueios.begin(); it != g_bloqueios.end();) {
                auto ator = it->handle.get();

                if (!ator || !EInvocacaoAliada(ator.get())) {
                    RestaurarAtaques(*it);
                    it = g_bloqueios.erase(it);
                } else {
                    ++it;
                }
            }

            // Procura novas invocacoes entre os atores que o jogo esta processando.
            auto* listas = RE::ProcessLists::GetSingleton();
            if (!listas) {
                return;
            }

            for (const auto& handle : listas->highActorHandles) {
                auto ator = handle.get();
                if (!ator || !EInvocacaoAliada(ator.get()) || JaBloqueado(handle)) {
                    continue;
                }

                // Confere a interface de AVs antes de registrar ou alterar o ator.
                auto* valores = ator->AsActorValueOwner();
                if (!valores) {
                    continue;
                }

                auto& flags = ator->GetActorRuntimeData().boolFlags;
                // Guarda o estado original para uma futura liberacao de combate.
                g_bloqueios.push_back({
                    handle,
                    flags.all(kAtaqueDesabilitado),
                    flags.all(kConjuracaoDesabilitada)
                });

                // Impede que a invocacao ataque ou conjure enquanto estiver bloqueada.
                flags.set(kAtaqueDesabilitado);
                flags.set(kConjuracaoDesabilitada);

                // Enquanto o combate esta bloqueado, a invocacao nao deve
                // iniciar hostilidades nem entrar para ajudar outros atores.
                // Os valores originais pertencem a esta instancia do ator.
                auto& bloqueio = g_bloqueios.back();
                bloqueio.agressividadeOriginal =
                    valores->GetActorValue(RE::ActorValue::kAggression);

                bloqueio.assistenciaOriginal =
                    valores->GetActorValue(RE::ActorValue::kAssistance);
                // Zera somente os AVs diferentes de zero e registra a alteracao.
                if (bloqueio.agressividadeOriginal != 0.0f) {
                    valores->SetActorValue(RE::ActorValue::kAggression, 0.0f);
                    bloqueio.agressividadeAlterada = true;
                }
                if (bloqueio.assistenciaOriginal != 0.0f) {
                    valores->SetActorValue(RE::ActorValue::kAssistance, 0.0f);
                    bloqueio.assistenciaAlterada = true;
                }
            }
        }

        // Identifica inimigos vivos que nao pertencem ao grupo do jogador.
        bool EInimigoValido(RE::Actor* actor, RE::Actor* player)
        {
            return actor && actor != player && !actor->IsDead() &&
                   !actor->IsPlayerTeammate() && !EInvocacaoAliada(actor) &&
                   actor->IsHostileToActor(player);
        }

        // Verifica se algum inimigo ativo ja detectou o jogador.
        bool JogadorDetectadoPorInimigo(RE::Actor* player)
        {
            auto* listas = RE::ProcessLists::GetSingleton();
            if (!listas || !player) {
                return false;
            }

            for (const auto& handle : listas->highActorHandles) {
                auto ator = handle.get();
                if (ator && EInimigoValido(ator.get(), player) &&
                    // Deteccao positiva libera o combate para as invocacoes.
                    ator->RequestDetectionLevel(player) > 0) {
                    return true;
                }
            }
            return false;
        }

        // Confere se um grupo inimigo mira o jogador ou algum aliado dele.
        bool GrupoTemAlvoAliado(RE::CombatGroup* grupo, RE::Actor* player)
        {
            if (!grupo) {
                return false;
            }
            std::vector<RE::ActorHandle> alvos;
            {
                // Copia os handles sob o lock; as consultas de IA ficam fora dele.
                RE::BSReadLockGuard trava(grupo->lock);
                alvos.reserve(grupo->targets.size());
                for (const auto& alvo : grupo->targets) {
                    alvos.push_back(alvo.targetHandle);
                }
            }
            // Consulta os alvos copiados sem manter o bloqueio de leitura do grupo.
            for (const auto& handle : alvos) {
                const auto alvoAtor = handle.get();
                if (alvoAtor &&
                    (alvoAtor.get() == player || alvoAtor->IsPlayerTeammate() ||
                     EInvocacaoAliada(alvoAtor.get()))) {
                    return true;
                }
            }
            return false;
        }

        // Evita adicionar um inimigo repetido ao confronto atual.
        bool InimigoJaRelacionado(const RE::ActorHandle& handle)
        {
            return std::find(g_inimigosConfronto.begin(),
                             g_inimigosConfronto.end(), handle) != g_inimigosConfronto.end();
        }

        // Guarda os inimigos ativos que estao lutando contra o grupo do jogador.
        void RegistrarInimigosDoConfronto(RE::Actor* player)
        {
            auto* listas = RE::ProcessLists::GetSingleton();
            if (!listas || !player) {
                return;
            }
            for (const auto& handle : listas->highActorHandles) {
                const auto ator = handle.get();
                if (!ator || !ator->IsInCombat() || !EInimigoValido(ator.get(), player) ||
                    InimigoJaRelacionado(handle)) {
                    continue;
                }
                // So associa o inimigo se o alvo dele participa do confronto.
                if (GrupoTemAlvoAliado(ator->GetCombatGroup(), player)) {
                    g_inimigosConfronto.push_back(handle);
                }
            }
        }

        // Verifica se o confronto ainda tem inimigos ativos antes de voltar a bloquear.
        bool HaInimigosCombatendo(RE::Actor* player)
        {
            auto* listas = RE::ProcessLists::GetSingleton();
            if (!listas || !player) {
                return false;
            }

            // Prioriza integrantes identificados do confronto, nao lutas
            // independentes entre hostis no mesmo high-actor list.
            bool haCombateHostilProximo = false;
            for (const auto& handle : listas->highActorHandles) {
                const auto ator = handle.get();
                if (!ator || !ator->IsInCombat() || !EInimigoValido(ator.get(), player)) {
                    continue;
                }
                // Marca que ha inimigo ativo, mesmo se a lista de alvos estiver indisponivel.
                haCombateHostilProximo = true;
                if (InimigoJaRelacionado(handle) ||
                    GrupoTemAlvoAliado(ator->GetCombatGroup(), player)) {
                    return true;
                }
            }
            // Compatibilidade: se o motor nao forneceu grupos/alvos,
            // conserva o criterio anterior em vez de bloquear precocemente.
            return g_inimigosConfronto.empty() && haCombateHostilProximo;
        }
        // Uma tarefa por vez, tanto no evento quanto no monitor periodico.
        void AgendarAtualizacao()
        {
            // Evita acumular atualizacoes iguais na fila de tarefas do SKSE.
            if (g_tarefaPendente.exchange(true)) {
                return;
            }
            // Captura a sessao atual para descartar tarefas antigas.
            const auto sessao = g_geracao.load();
            auto* tarefas = SKSE::GetTaskInterface();
            if (!tarefas) {
                g_tarefaPendente.store(false);
                return;
            }
            // Executa a atualizacao na thread principal, se a sessao ainda for valida.
            tarefas->AddTask([sessao] {
                if (sessao != g_geracao.load()) {
                    return;
                }
                g_tarefaPendente.store(false);
                AtualizarControleCombate();
            });
        }
    }

    // Decide quando as invocacoes podem lutar com base no combate do jogador.
    void AtualizarControleCombate()
    {
        // Primeiro recupera valores salvos antes de reaplicar qualquer bloqueio.
        RestaurarAVsPendentes();
        if (!g_eInvocacaoAliada) {
            return;
        }

        // Sem jogador carregado, nao consulta deteccao nem altera o combate.
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            return;
        }

        // So a primeira deteccao ou a entrada em combate libera os ataques.
        // Depois, o combate permanece liberado ate o fim do confronto.
        const bool gatilho = player->IsInCombat() || JogadorDetectadoPorInimigo(player);
        if (g_combateLiberado) {
            RegistrarInimigosDoConfronto(player);
        }

        // Ao comecar a luta, libera as invocacoes e restaura seus valores.
        if (gatilho && !g_combateLiberado) {
            g_inimigosConfronto.clear();
            g_combateLiberado = true;
            RegistrarInimigosDoConfronto(player);
            RestaurarTodos();
            LogInfo("Combate liberado para invocacoes aliadas",
                    "Combat enabled for allied summons");
        } else if (!gatilho && g_combateLiberado && !HaInimigosCombatendo(player)) {
            // Quando o confronto acaba, volta a impedir novas hostilidades.
            g_combateLiberado = false;
            LogInfo("Combate bloqueado para invocacoes aliadas",
                    "Combat disabled for allied summons");
        }

        // Aplica o estado atual: restaurar no combate ou bloquear fora dele.
        if (g_combateLiberado) {
            RestaurarTodos();
        } else {
            SincronizarBloqueios();
        }
    }

    // Encerra o monitor, remove os eventos e restaura as invocacoes.
    void PararControleCombate()
    {
        // Invalida as tarefas pendentes de uma partida anterior.
        ++g_geracao;
        // Desregistra o ouvinte para nao receber eventos de uma sessao encerrada.
        if (g_fonteEventosCombate) {
            g_fonteEventosCombate->RemoveEventSink<RE::TESCombatEvent>(&g_sinkCombate);
            g_fonteEventosCombate = nullptr;
        }
        // Para a thread do monitor antes de limpar os estados guardados.
        if (g_monitor.joinable()) {
            g_monitor.request_stop();
            g_monitor.join();
        }

        g_tarefaPendente.store(false);
        RestaurarTodos();
        g_combateLiberado = false;
        g_inimigosConfronto.clear();
        g_eInvocacaoAliada = {};
    }

    // Liga ao SKSE as rotinas para salvar, carregar e descartar AVs.
    void RegistrarSerializacaoAV()
    {
        auto* serializacao = SKSE::GetSerializationInterface();
        if (!serializacao) {
            LogErro("Serializacao SKSE indisponivel para os AVs",
                    "SKSE serialization unavailable for actor values");
            return;
        }
        // Informa ao SKSE qual identificador e quais callbacks utilizar.
        serializacao->SetUniqueID(kIdentificadorSerializacao);
        serializacao->SetSaveCallback(SalvarAVs);
        serializacao->SetLoadCallback(CarregarAVs);
        serializacao->SetRevertCallback(ReverterAVs);
    }

    // Reinicia o controle e comeca a acompanhar eventos e combates periodicamente.
    void IniciarControleCombate(std::function<bool(RE::Actor*)> a_eInvocacaoAliada)
    {
        // Remove qualquer monitor anterior e guarda o novo filtro de invocacoes.
        PararControleCombate();
        g_eInvocacaoAliada = std::move(a_eInvocacaoAliada);
        // Passa a receber notificacoes sempre que o combate mudar.
        g_fonteEventosCombate = RE::ScriptEventSourceHolder::GetSingleton();
        if (g_fonteEventosCombate) {
            g_fonteEventosCombate->AddEventSink<RE::TESCombatEvent>(&g_sinkCombate);
        }

        const auto sessao = ++g_geracao;
        AtualizarControleCombate();

        // A cada intervalo, agenda uma verificacao sem acessar atores nesta thread.
        g_monitor = std::jthread([sessao](std::stop_token stop) {
            while (!stop.stop_requested()) {
                std::this_thread::sleep_for(kIntervaloMonitor);
                if (stop.stop_requested() || sessao != g_geracao.load()) {
                    break;
                }

                AgendarAtualizacao();
            }
        });
    }
}
