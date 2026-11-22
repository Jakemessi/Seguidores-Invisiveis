#include "PCH.h"
#include "CombatLock.h"
#include "Logger.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <thread>

namespace IAFT
{
    namespace
    {
        constexpr auto kIntervaloMonitor = std::chrono::milliseconds(400);
        constexpr auto kAtaqueDesabilitado = RE::Actor::BOOL_FLAGS::kAttackingDisabled;
        constexpr auto kConjuracaoDesabilitada = RE::Actor::BOOL_FLAGS::kCastingDisabled;
        constexpr std::uint32_t kIdentificadorSerializacao = 0x54464149; // IAFT
        constexpr std::uint32_t kTipoRegistroAV = 0x56414941;           // AIAV
        constexpr std::uint32_t kVersaoRegistroAV = 1;
        constexpr std::uint32_t kAVAlteradoAggression = 1;
        constexpr std::uint32_t kAVAlteradoAssistance = 2;

        struct RegistroAVPersistido
        {
            std::uint32_t formID = 0;
            float agressividadeOriginal = 0.0f;
            float assistenciaOriginal = 0.0f;
            std::uint32_t flags = 0;
        };

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

        std::vector<Bloqueio> g_bloqueios;
        std::vector<RegistroAVPersistido> g_avPendentes;
        std::function<bool(RE::Actor*)> g_eInvocacaoAliada;
        bool g_combateLiberado = false;

        // O monitor apenas agenda tarefas. Todos os acessos a atores/flags
        // acontecem nas tarefas da thread principal do Skyrim.
        std::atomic<std::uint64_t> g_geracao{ 0 };
        std::atomic<bool> g_tarefaPendente{ false };
        RE::ScriptEventSourceHolder* g_fonteEventosCombate = nullptr;

        void AgendarAtualizacao();

        class EventoCombate final : public RE::BSTEventSink<RE::TESCombatEvent>
        {
        public:
            RE::BSEventNotifyControl ProcessEvent(
                const RE::TESCombatEvent* evento,
                RE::BSTEventSource<RE::TESCombatEvent>*) override
            {
                if (evento) {
                    AgendarAtualizacao();
                }
                return RE::BSEventNotifyControl::kContinue;
            }
        };

        EventoCombate g_sinkCombate;
        // Declarado por ultimo para ser destruido primeiro no encerramento.
        std::jthread g_monitor;

        void SalvarAVs(SKSE::SerializationInterface* serializacao)
        {
            std::vector<RegistroAVPersistido> registros = g_avPendentes;
            for (const auto& bloqueio : g_bloqueios) {
                const auto ator = bloqueio.handle.get();
                if (!ator || (!bloqueio.agressividadeAlterada && !bloqueio.assistenciaAlterada)) {
                    continue;
                }

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

            if (registros.empty() || !serializacao->OpenRecord(kTipoRegistroAV, kVersaoRegistroAV)) {
                return;
            }

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

        void CarregarAVs(SKSE::SerializationInterface* serializacao)
        {
            g_avPendentes.clear();
            std::uint32_t tipo = 0;
            std::uint32_t versao = 0;
            std::uint32_t tamanho = 0;
            while (serializacao->GetNextRecordInfo(tipo, versao, tamanho)) {
                if (tipo != kTipoRegistroAV || versao != kVersaoRegistroAV ||
                    tamanho < sizeof(std::uint32_t)) {
                    continue;
                }

                std::uint32_t quantidade = 0;
                if (serializacao->ReadRecordData(quantidade) != sizeof(quantidade) ||
                    quantidade > 4096 ||
                    quantidade > (tamanho - sizeof(quantidade)) / sizeof(RegistroAVPersistido)) {
                    LogErro("Registro invalido de AVs no co-save",
                            "Invalid actor-value record in SKSE co-save");
                    continue;
                }

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

        void ReverterAVs(SKSE::SerializationInterface*)
        {
            g_avPendentes.clear();
        }

        void RestaurarAVsPendentes()
        {
            for (auto it = g_avPendentes.begin(); it != g_avPendentes.end();) {
                auto* ator = RE::TESForm::LookupByID<RE::Actor>(it->formID);
                if (!ator) {
                    ++it; // Uma invocacao ainda pode nao ter carregado.
                    continue;
                }

                // Nunca sobrescreve a modificacao de outro mod.
                if ((it->flags & kAVAlteradoAggression) &&
                    ator->GetActorValue(RE::ActorValue::kAggression) == 0.0f) {
                    ator->SetActorValue(RE::ActorValue::kAggression, it->agressividadeOriginal);
                }
                if ((it->flags & kAVAlteradoAssistance) &&
                    ator->GetActorValue(RE::ActorValue::kAssistance) == 0.0f) {
                    ator->SetActorValue(RE::ActorValue::kAssistance, it->assistenciaOriginal);
                }
                it = g_avPendentes.erase(it);
            }
        }

        bool EInvocacaoAliada(RE::Actor* actor)
        {
            return actor && !actor->IsDead() &&
                   g_eInvocacaoAliada && g_eInvocacaoAliada(actor);
        }

        void RestaurarAtaques(const Bloqueio& bloqueio)
        {
            auto ator = bloqueio.handle.get();
            if (!ator) {
                return;
            }

            auto& flags = ator->GetActorRuntimeData().boolFlags;
            if (!bloqueio.ataqueJaDesabilitado) {
                flags.reset(kAtaqueDesabilitado);
            }
            if (!bloqueio.conjuracaoJaDesabilitada) {
                flags.reset(kConjuracaoDesabilitada);
            }

            // Restaura apenas os AVs modificados pela DLL. Se outro mod
            // alterou o valor enquanto estava bloqueado, respeita a alteracao.
            if (bloqueio.agressividadeAlterada &&
                ator->GetActorValue(RE::ActorValue::kAggression) == 0.0f) {
                ator->SetActorValue(RE::ActorValue::kAggression,
                                    bloqueio.agressividadeOriginal);
            }
            if (bloqueio.assistenciaAlterada &&
                ator->GetActorValue(RE::ActorValue::kAssistance) == 0.0f) {
                ator->SetActorValue(RE::ActorValue::kAssistance,
                                    bloqueio.assistenciaOriginal);
            }
        }

        void RestaurarTodos()
        {
            for (const auto& bloqueio : g_bloqueios) {
                RestaurarAtaques(bloqueio);
            }
            g_bloqueios.clear();
        }

        bool JaBloqueado(const RE::ActorHandle& handle)
        {
            return std::any_of(g_bloqueios.begin(), g_bloqueios.end(),
                [&handle](const Bloqueio& bloqueio) {
                    return bloqueio.handle == handle;
                });
        }

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

            auto* listas = RE::ProcessLists::GetSingleton();
            if (!listas) {
                return;
            }

            for (const auto& handle : listas->highActorHandles) {
                auto ator = handle.get();
                if (!ator || !EInvocacaoAliada(ator.get()) || JaBloqueado(handle)) {
                    continue;
                }

                auto& flags = ator->GetActorRuntimeData().boolFlags;
                g_bloqueios.push_back({
                    handle,
                    flags.all(kAtaqueDesabilitado),
                    flags.all(kConjuracaoDesabilitada)
                });

                flags.set(kAtaqueDesabilitado);
                flags.set(kConjuracaoDesabilitada);

                // Enquanto o combate esta bloqueado, a invocacao nao deve
                // iniciar hostilidades nem entrar para ajudar outros atores.
                // Os valores originais pertencem a esta instancia do ator.
                auto& bloqueio = g_bloqueios.back();
                bloqueio.agressividadeOriginal =
                    ator->GetActorValue(RE::ActorValue::kAggression);
                bloqueio.assistenciaOriginal =
                    ator->GetActorValue(RE::ActorValue::kAssistance);

                if (bloqueio.agressividadeOriginal != 0.0f) {
                    ator->SetActorValue(RE::ActorValue::kAggression, 0.0f);
                    bloqueio.agressividadeAlterada = true;
                }
                if (bloqueio.assistenciaOriginal != 0.0f) {
                    ator->SetActorValue(RE::ActorValue::kAssistance, 0.0f);
                    bloqueio.assistenciaAlterada = true;
                }
            }
        }

        bool EInimigoValido(RE::Actor* actor, RE::Actor* player)
        {
            return actor && actor != player && !actor->IsDead() &&
                   !actor->IsPlayerTeammate() && !EInvocacaoAliada(actor) &&
                   actor->IsHostileToActor(player);
        }

        bool JogadorDetectadoPorInimigo(RE::Actor* player)
        {
            auto* listas = RE::ProcessLists::GetSingleton();
            if (!listas || !player) {
                return false;
            }

            for (const auto& handle : listas->highActorHandles) {
                auto ator = handle.get();
                if (ator && EInimigoValido(ator.get(), player) &&
                    ator->RequestDetectionLevel(player) > 0) {
                    return true;
                }
            }
            return false;
        }

        bool HaInimigosCombatendo(RE::Actor* player)
        {
            auto* listas = RE::ProcessLists::GetSingleton();
            if (!listas || !player) {
                return false;
            }

            // Mantem a liberacao enquanto houver hostis proximos em combate,
            // mesmo quando o jogador ja saiu de combate ou voltou a se ocultar.
            for (const auto& handle : listas->highActorHandles) {
                auto ator = handle.get();
                if (ator && EInimigoValido(ator.get(), player) && ator->IsInCombat()) {
                    return true;
                }
            }
            return false;
        }
        // Uma tarefa por vez, tanto no evento quanto no monitor periodico.
        void AgendarAtualizacao()
        {
            if (g_tarefaPendente.exchange(true)) {
                return;
            }
            const auto sessao = g_geracao.load();
            auto* tarefas = SKSE::GetTaskInterface();
            if (!tarefas) {
                g_tarefaPendente.store(false);
                return;
            }
            tarefas->AddTask([sessao] {
                if (sessao != g_geracao.load()) {
                    return;
                }
                g_tarefaPendente.store(false);
                AtualizarControleCombate();
            });
        }
    }

    void AtualizarControleCombate()
    {
        // Primeiro recupera valores salvos antes de reaplicar qualquer bloqueio.
        RestaurarAVsPendentes();
        if (!g_eInvocacaoAliada) {
            return;
        }

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            return;
        }

        // So a primeira deteccao ou a entrada em combate libera os ataques.
        // Depois, o combate permanece liberado ate o fim do confronto.
        const bool gatilho = player->IsInCombat() || JogadorDetectadoPorInimigo(player);

        if (gatilho && !g_combateLiberado) {
            g_combateLiberado = true;
            RestaurarTodos();
            LogInfo("Combate liberado para invocacoes aliadas",
                    "Combat enabled for allied summons");
        } else if (!gatilho && g_combateLiberado && !HaInimigosCombatendo(player)) {
            g_combateLiberado = false;
            LogInfo("Combate bloqueado para invocacoes aliadas",
                    "Combat disabled for allied summons");
        }

        if (g_combateLiberado) {
            RestaurarTodos();
        } else {
            SincronizarBloqueios();
        }
    }

    void PararControleCombate()
    {
        // Invalida as tarefas pendentes de uma partida anterior.
        ++g_geracao;
        if (g_fonteEventosCombate) {
            g_fonteEventosCombate->RemoveEventSink<RE::TESCombatEvent>(&g_sinkCombate);
            g_fonteEventosCombate = nullptr;
        }
        if (g_monitor.joinable()) {
            g_monitor.request_stop();
            g_monitor.join();
        }

        g_tarefaPendente.store(false);
        RestaurarTodos();
        g_combateLiberado = false;
        g_eInvocacaoAliada = {};
    }

    void RegistrarSerializacaoAV()
    {
        auto* serializacao = SKSE::GetSerializationInterface();
        if (!serializacao) {
            LogErro("Serializacao SKSE indisponivel para os AVs",
                    "SKSE serialization unavailable for actor values");
            return;
        }
        serializacao->SetUniqueID(kIdentificadorSerializacao);
        serializacao->SetSaveCallback(SalvarAVs);
        serializacao->SetLoadCallback(CarregarAVs);
        serializacao->SetRevertCallback(ReverterAVs);
    }

    void IniciarControleCombate(std::function<bool(RE::Actor*)> a_eInvocacaoAliada)
    {
        PararControleCombate();
        g_eInvocacaoAliada = std::move(a_eInvocacaoAliada);
        g_fonteEventosCombate = RE::ScriptEventSourceHolder::GetSingleton();
        if (g_fonteEventosCombate) {
            g_fonteEventosCombate->AddEventSink<RE::TESCombatEvent>(&g_sinkCombate);
        }

        const auto sessao = ++g_geracao;
        AtualizarControleCombate();

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
