#include "PCH.h"
#include "Version.h"
#include "Logger.h"
#include "CombatLock.h"

#include <algorithm>

// Guarda o estado do plugin e as funcoes usadas somente neste arquivo.
namespace
{
    // Estado do plugin
    bool g_stealthAtivado = false;
    constexpr bool p_debug = false;

    // Indica se as flags de invisibilidade estão atualmente aplicadas.
    bool g_flagInvisibilidadeAtiva = false;

    // Valores aplicados pelo plugin
    constexpr float g_sneakBoost = 1000.0f;
    constexpr float g_invisibilityBoost = 0.0f;

    // Camada do Actor Value usada pelo plugin.
    // kTemporary indica que o boost deve ser tratado como modificador temporário,
    // em vez de uma alteração permanente no valor do ator, assim sendo até mais fácil de remover.
    constexpr RE::ACTOR_VALUE_MODIFIER g_modificadorActorValue = RE::ACTOR_VALUE_MODIFIER::kTemporary;

    // Ator individual, nunca o ActorBase compartilhado com NPCs comuns.
    std::vector<RE::ActorHandle> g_aliados;

    // Registra a flag original de cada aliado para restaura-la depois.
    struct AliadoFlagStealth
    {
        RE::ActorHandle handle;
        bool jaNaoApareciaNoStealthMeter = false;
    };

    // Mantem os aliados cuja flag de furtividade foi acompanhada.
    std::vector<AliadoFlagStealth> g_flagsStealth;

    // Obtem um nome seguro para mostrar nas mensagens do log.
    std::string GetActorDisplayName(RE::Actor* actor)
    {
        if (!actor) {
            return VaiBrasa() ? "Ator desconhecido" : "Unknown actor";
        }

        // Se o ator nao tiver nome, usa um texto padrao no idioma escolhido.
        const char* name = actor->GetName();

        if (!name || name[0] == '\0') {
            return VaiBrasa() ? "Ator sem nome" : "Unnamed actor";
        }

        return std::string(name);
    }


    // ------------------------------------------------------------
    // Verificar se o jogador está invisivel
    // ------------------------------------------------------------

    // Detecta se o jogador possui um efeito magico de invisibilidade ativo.
    bool JogadorEstaInvisivel()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();

        if (!player) {
            return false;
        }

        // Consulta a lista de efeitos magicos do jogador.
        auto* magicTarget = player->AsMagicTarget();

        return magicTarget &&
            magicTarget->HasEffectWithArchetype(
                RE::EffectArchetype::kInvisibility
            );
    }

    // ------------------------------------------------------------
    // Busca de seguidores
    // ------------------------------------------------------------

    // Considera seguidor qualquer companheiro do jogador, exceto o proprio jogador.
    bool IsFollower(RE::Actor* actor)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        return actor && actor != player && actor->IsPlayerTeammate();
    }

    // Invocações, comandados e reanimados pertencentes ao jogador ou a seguidores.
    // Um NPC amigável sem essa relação não entra no filtro.
    // Aceita invocacoes, comandados e reanimados controlados pelo grupo do jogador.
    bool IsInvocacaoAliada(RE::Actor* actor)
    {
        if (!actor || (!actor->IsSummoned() && !actor->IsCommandedActor())) {
            return false;
        }

        // Primeiro verifica quem esta comandando diretamente a criatura.
        auto comandante = actor->GetCommandingActor();
        auto* dono = comandante.get();
        auto* player = RE::PlayerCharacter::GetSingleton();

        // O controlador precisa ser o jogador ou um de seus seguidores.
        const auto aliado = [&](RE::Actor* candidato) {
            return candidato && candidato != actor &&
                   (candidato == player || IsFollower(candidato));
        };
        // Um comandante aliado ja confirma que a invocacao pertence ao grupo.
        if (aliado(dono)) {
            return true;
        }

        // API introduzida na CommonLibSSE-NG 7.5.0. O wrapper atual
        // nao possui relocation especifica para Skyrim VR.
        if (!REL::Module::IsVR()) {
            RE::NiPointer<RE::Actor> controlador;
            if (actor->GetControllingActor(controlador) && aliado(controlador.get())) {
                return true;
            }
        }
        return false;
    }

    // Une os filtros de seguidores comuns e invocacoes aliadas.
    bool IsAliado(RE::Actor* actor)
    {
        return IsFollower(actor) || IsInvocacaoAliada(actor);
    }

    // Executa uma acao para cada aliado ativo na lista de atores do jogo.
    void ForEachAliado(const std::function<void(RE::Actor*, const RE::ActorHandle&)>& callback)
    {
        auto* processLists = RE::ProcessLists::GetSingleton();
        if (!processLists) {
            return;
        }

        // Percorre os atores carregados e ignora referencias indisponiveis.
        for (const auto& handle : processLists->highActorHandles) {
            auto actorPtr = handle.get();
            if (!actorPtr) {
                continue;
            }

            auto* actor = actorPtr.get();
            // So executa a acao para quem pertence ao grupo do jogador.
            if (IsAliado(actor)) {
                callback(actor, handle);
            }
        }
    }


    // ------------------------------------------------------------
    // Aplicação e remoção dos efeitos
    // ------------------------------------------------------------
    // Métodos de aplicação e remoção de AVs Temporários
    // Aplica o bonus temporario de Sneak e o modificador configurado de invisibilidade.
    bool AplicarEfeitos(RE::Actor* actor)
    {
        if (!actor) {
            return false;
        }

        // Consulta a interface de Actor Values; sem ela nao modifica o ator.
        auto* actorValueOwner = actor->AsActorValueOwner();

        if (!actorValueOwner) {
            return false;
        }

        // Soma o bonus configurado ao Sneak temporario do aliado.
        actorValueOwner->ModActorValue(
            g_modificadorActorValue,
            RE::ActorValue::kSneak,
            g_sneakBoost
        );

        // Aplica o valor de invisibilidade configurado (atualmente zero).
        actorValueOwner->ModActorValue(
            g_modificadorActorValue,
            RE::ActorValue::kInvisibility,
            g_invisibilityBoost
        );

        return true;
    }

    // Compensa os bonus temporarios que foram aplicados ao aliado.
    bool RemoverEfeitos(RE::Actor* actor)
    {
        if (!actor) {
            return false;
        }

        auto* actorValueOwner = actor->AsActorValueOwner();

        if (!actorValueOwner) {
            return false;
        }

        // Subtrai o mesmo bonus de Sneak para recuperar o valor anterior.
        actorValueOwner->ModActorValue(
            g_modificadorActorValue,
            RE::ActorValue::kSneak,
            -g_sneakBoost
        );

        // Compensa o modificador de invisibilidade aplicado anteriormente.
        actorValueOwner->ModActorValue(
            g_modificadorActorValue,
            RE::ActorValue::kInvisibility,
            -g_invisibilityBoost
        );

        return true;
    }

    // Evita aplicar duas vezes o bonus a um aliado ja acompanhado.
    bool AliadoJaRegistrado(const RE::ActorHandle& handle)
    {
        return std::find(g_aliados.begin(), g_aliados.end(), handle) != g_aliados.end();
    }

    // Verifica se a flag de furtividade deste aliado ja foi salva.
    bool FlagJaRegistrada(const RE::ActorHandle& handle)
    {
        return std::find_if(
            g_flagsStealth.begin(), g_flagsStealth.end(),
            [&handle](const AliadoFlagStealth& registro) {
                return registro.handle == handle;
            }
        ) != g_flagsStealth.end();
    }

    // Oculta o aliado do indicador de deteccao sem mexer em outros NPCs.
    void AplicarFlagNaoAfetaStealth(RE::Actor* actor, const RE::ActorHandle& handle)
    {
        if (!actor || FlagJaRegistrada(handle)) {
            return;
        }

        // Flag por instância: não modifica o TESNPC compartilhado por outros NPCs.
        auto& flags = actor->GetActorRuntimeData().boolFlags;
        constexpr auto flag = RE::Actor::BOOL_FLAGS::kDoNotShowOnStealthMeter;
        const bool jaNaoAparecia = flags.all(flag);
        g_flagsStealth.push_back({ handle, jaNaoAparecia });

        // So ativa a flag quando ela nao estava ativada antes.
        if (!jaNaoAparecia) {
            flags.set(flag);
        }

        const auto nome = GetActorDisplayName(actor);
        LogInfo(nome + " nao afeta mais o stealth meter",
                nome + " no longer affects the stealth meter");
    }

    // Atualiza as flags de furtividade quando aliados entram ou saem do grupo.
    void SincronizarFlagsInvisibilidade()
    {
        constexpr auto flag = RE::Actor::BOOL_FLAGS::kDoNotShowOnStealthMeter;

        // Uma reanimação pode perder o comandante antes do fim da invisibilidade.
        for (auto it = g_flagsStealth.begin(); it != g_flagsStealth.end();) {
            auto actorPtr = it->handle.get();
            if (actorPtr && !IsAliado(actorPtr.get())) {
                // Restaura a flag anterior quando um ator deixa de ser aliado.
                if (!it->jaNaoApareciaNoStealthMeter) {
                    actorPtr->GetActorRuntimeData().boolFlags.reset(flag);
                }
                it = g_flagsStealth.erase(it);
            } else {
                ++it;
            }
        }

        // Aplica a flag aos aliados que surgiram depois da ultima verificacao.
        ForEachAliado([](RE::Actor* actor, const RE::ActorHandle& handle) {
            AplicarFlagNaoAfetaStealth(actor, handle);
        });
    }

    // Desfaz as flags de furtividade aplicadas enquanto havia invisibilidade.
    void RemoverFlagsInvisibilidade()
    {
        if (!g_flagInvisibilidadeAtiva && g_flagsStealth.empty()) {
            return;
        }

        constexpr auto flag = RE::Actor::BOOL_FLAGS::kDoNotShowOnStealthMeter;
        // Restaura somente as flags que estavam desligadas originalmente.
        for (const auto& registro : g_flagsStealth) {
            auto actorPtr = registro.handle.get();
            if (actorPtr && !registro.jaNaoApareciaNoStealthMeter) {
                actorPtr->GetActorRuntimeData().boolFlags.reset(flag);
            }
        }

        // Esquece os registros depois de restaurar as flags.
        g_flagsStealth.clear();
        g_flagInvisibilidadeAtiva = false;

        LogInfo("Flags de stealth dos aliados restauradas",
                "Ally stealth flags restored");
    }

    // Liga as flags quando ha invisibilidade e as remove ao terminar o efeito.
    void AtualizarFlagsInvisibilidade(bool estaInvisivel)
    {
        if (estaInvisivel) {
            // Na primeira ativacao, inicia o acompanhamento das flags.
            if (!g_flagInvisibilidadeAtiva) {
                g_flagInvisibilidadeAtiva = true;
                g_flagsStealth.clear();

                LogInfo(
                    "Invisibilidade detectada: aliados nao afetarao o stealth meter",
                    "Invisibility detected: allies will not affect the stealth meter"
                );
            }

            SincronizarFlagsInvisibilidade();
        }
        else if (g_flagInvisibilidadeAtiva) {
            // Ao acabar a invisibilidade, devolve as flags originais.
            RemoverFlagsInvisibilidade();
        }
    }

    // Retira os bonus de ex-aliados e aplica bonus a novos aliados.
    void SincronizarSeguidores()
    {
        // Descarta comandados que deixaram de pertencer ao jogador/grupo.
        for (auto it = g_aliados.begin(); it != g_aliados.end();) {
            auto actorPtr = it->get();
            if (actorPtr && !IsAliado(actorPtr.get())) {
                // Um ator que saiu do grupo perde o modificador temporario.
                RemoverEfeitos(actorPtr.get());
                it = g_aliados.erase(it);
            } else {
                ++it;
            }
        }

        // Aplica o bonus somente a aliados ainda nao registrados.
        ForEachAliado([](RE::Actor* actor, const RE::ActorHandle& handle) {
            if (AliadoJaRegistrado(handle) || !AplicarEfeitos(actor)) {
                return;
            }

            g_aliados.push_back(handle);
            const auto nome = GetActorDisplayName(actor);
            LogInfo(nome + " recebeu boost de furtividade",
                    nome + " received sneak boost");
        });
    }

    // Inicia os efeitos quando o jogador entra em furtividade ou invisibilidade.
    void AtivarEfeitosSeguidores()
    {
        if (g_stealthAtivado) {
            return;
        }

        // Reinicia a lista antes de localizar e modificar os aliados ativos.
        g_stealthAtivado = true;
        g_aliados.clear();

        SincronizarSeguidores();
    }

    // Remove os bonus temporarios ao sair da furtividade.
    void DesativarEfeitosSeguidores()
    {
        if (!g_stealthAtivado) {
            return;
        }

        g_stealthAtivado = false;

        // Recupera cada aliado ainda carregado para retirar os efeitos.
        for (const auto& handle : g_aliados) {
            auto actorPtr = handle.get();

            if (!actorPtr) {
                continue;
            }

            auto* actor = actorPtr.get();

            if (!actor) {
                continue;
            }

            if (!RemoverEfeitos(actor)) {
                continue;
            }

            const auto actorName = GetActorDisplayName(actor);

            LogInfo(
                actorName + " perdeu boost de furtividade",
                actorName + " lost sneak boost"
            );
        }

        // Limpa o acompanhamento depois de tentar remover todos os bonus.
        g_aliados.clear();
    }

    // Atualiza novas criaturas enquanto a furtividade já está ativada.
    void SincronizarAliadosAtivos()
    {
        if (g_stealthAtivado) {
            SincronizarSeguidores();
        }
        if (g_flagInvisibilidadeAtiva) {
            SincronizarFlagsInvisibilidade();
        }
    }

    // Recebe avisos quando atores carregam ou efeitos magicos sao aplicados.
    class AllyEventSink :
        public RE::BSTEventSink<RE::TESObjectLoadedEvent>,
        public RE::BSTEventSink<RE::TESMagicEffectApplyEvent>
    {
    public:
        // Usa uma unica instancia para receber todos os eventos de aliados.
        static AllyEventSink* GetSingleton()
        {
            static AllyEventSink singleton;
            return std::addressof(singleton);
        }

        // Reage a um ator carregado enquanto os efeitos estao ativos.
        RE::BSEventNotifyControl ProcessEvent(
            const RE::TESObjectLoadedEvent* event,
            RE::BSTEventSource<RE::TESObjectLoadedEvent>*) override
        {
            if (!event || !event->loaded ||
                (!g_stealthAtivado && !g_flagInvisibilidadeAtiva)) {
                return RE::BSEventNotifyControl::kContinue;
            }

            // Se o ator carregado for aliado, atualiza os bonus e o combate.
            auto* actor = RE::TESForm::LookupByID<RE::Actor>(event->formID);
            if (IsAliado(actor)) {
                SincronizarAliadosAtivos();
                IAFT::AtualizarControleCombate();
            }
            return RE::BSEventNotifyControl::kContinue;
        }

        // Reage a novos efeitos magicos que possam mudar o grupo de aliados.
        RE::BSEventNotifyControl ProcessEvent(
            const RE::TESMagicEffectApplyEvent* event,
            RE::BSTEventSource<RE::TESMagicEffectApplyEvent>*) override
        {
            if (!event || (!g_stealthAtivado && !g_flagInvisibilidadeAtiva)) {
                return RE::BSEventNotifyControl::kContinue;
            }

            // Identifica quem lancou o efeito e quem recebeu a magia.
            auto* caster = event->caster ? event->caster->As<RE::Actor>() : nullptr;
            auto* target = event->target ? event->target->As<RE::Actor>() : nullptr;
            auto* player = RE::PlayerCharacter::GetSingleton();

            // Atualiza os aliados se a magia envolver o jogador ou seu grupo.
            if (IsAliado(target) || caster == player || IsFollower(caster)) {
                SincronizarAliadosAtivos();
                IAFT::AtualizarControleCombate();
            }

            return RE::BSEventNotifyControl::kContinue;
        }
    };

    // Registra os eventos de carregamento de atores e aplicacao de magia.
    void RegisterAllyEvents()
    {
        auto* sources = RE::ScriptEventSourceHolder::GetSingleton();
        if (!sources) {
            LogErro("Fontes de evento indisponiveis", "Event sources unavailable");
            return;
        }

        // Remove registros antigos para nao receber eventos duplicados.
        sources->RemoveEventSink<RE::TESObjectLoadedEvent>(AllyEventSink::GetSingleton());
        sources->RemoveEventSink<RE::TESMagicEffectApplyEvent>(AllyEventSink::GetSingleton());
        // Adiciona os ouvintes atualizados ao jogo.
        sources->AddEventSink<RE::TESObjectLoadedEvent>(AllyEventSink::GetSingleton());
        sources->AddEventSink<RE::TESMagicEffectApplyEvent>(AllyEventSink::GetSingleton());
    }

    // ------------------------------------------------------------
    // Eventos de animação
    // ------------------------------------------------------------
    // Acompanha as animacoes do jogador para detectar mudancas de furtividade.
    class AnimationEventSink :
        public RE::BSTEventSink<RE::BSAnimationGraphEvent>
    {
    public:
        // Reutiliza o mesmo ouvinte dos eventos de animacao.
        static AnimationEventSink* GetSingleton()
        {
            static AnimationEventSink singleton;
            return std::addressof(singleton);
        }
        
        // Recebe o evento e ignora animacoes que nao sao do jogador.
        RE::BSEventNotifyControl ProcessEvent(
            const RE::BSAnimationGraphEvent* event,
            RE::BSTEventSource<RE::BSAnimationGraphEvent>*)
            override
        {
            if (!event) {
                return RE::BSEventNotifyControl::kContinue;
            }

            auto* player = RE::PlayerCharacter::GetSingleton();

            if (!player || event->holder != player) {
                return RE::BSEventNotifyControl::kContinue;
            }

            // Exibe o nome do evento somente quando o modo de debug esta ligado.
            if constexpr (p_debug) {
                const std::string tag = event->tag.c_str();

                LogInfo(
                    "Evento de animacao recebido: " + tag,
                    "Animation event received: " + tag
                );
            }

            // Consulta agachamento e invisibilidade para definir o estado de stealth.
            const bool estaAgachado = player->IsSneaking();
            const bool estaInvisivel = JogadorEstaInvisivel();

            // Atualiza separadamente a flag que afeta o indicador de deteccao.
            AtualizarFlagsInvisibilidade(estaInvisivel);

            const bool EstaEmStealth = estaAgachado || estaInvisivel;

            // Registra o estado completo apenas para diagnosticos de debug.
            if constexpr (p_debug) {
                LogInfo(
                    "Estado do player: IsSneaking=" +
                        std::string(estaAgachado ? "true" : "false") +
                        ", IsInvisible=" +
                        std::string(estaInvisivel ? "true" : "false") +
                        ", g_stealthAtivado=" +
                        std::string(g_stealthAtivado ? "true" : "false") +
                        ", g_flagInvisibilidadeAtiva=" +
                        std::string(g_flagInvisibilidadeAtiva ? "true" : "false"),

                    "Player state: IsSneaking=" +
                        std::string(estaAgachado ? "true" : "false") +
                        ", IsInvisible=" +
                        std::string(estaInvisivel ? "true" : "false") +
                        ", g_stealthAtivado=" +
                        std::string(g_stealthAtivado ? "true" : "false") +
                        ", g_flagInvisibilidadeAtiva=" +
                        std::string(g_flagInvisibilidadeAtiva ? "true" : "false")
                );
            }

            // Com stealth ativo, inicia ou atualiza os efeitos dos aliados.
            if(EstaEmStealth) {
                if (!g_stealthAtivado) {

                    // Registra se o gatilho foi agachamento, invisibilidade ou ambos.
                    if (estaAgachado && estaInvisivel) {
                        LogInfo(
                            "Sneak e invisibilidade detectados",
                            "Sneak and invisibility detected"
                        );
                    }
                    else if (estaAgachado) {
                        LogInfo(
                            "Sneak detectado",
                            "Sneak detected"
                        );
                    }
                    else {
                        LogInfo(
                            "Invisibilidade detectada",
                            "Invisibility detected"
                        );
                    }

                    AtivarEfeitosSeguidores();
                }
                else {
                    SincronizarSeguidores();
                }
            }
            else if (g_stealthAtivado) {
                // Fora de stealth, retira os modificadores temporarios dos aliados.
                LogInfo(
                    "Saindo de stealth",
                    "Exiting stealth"
                );

                DesativarEfeitosSeguidores();
            }

            return RE::BSEventNotifyControl::kContinue;
        }
    };

    // Registra novamente o ouvinte de animacoes do jogador atual.
    void RegisterAnimationEvents()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();

        if (!player) {
            LogErro("Player não encontrado", "Player not found");
            return;
        }

        // Remove a inscricao antiga e adiciona a instancia atual uma unica vez.
        player->RemoveAnimationGraphEventSink(AnimationEventSink::GetSingleton());
        player->AddAnimationGraphEventSink(AnimationEventSink::GetSingleton());

        LogInfo(
            "AnimationGraphEvent registrado/re-registrado",
            "AnimationGraphEvent registered/re-registered"
        );
    }
}

// Entrada carregada pelo SKSE; metadados do plugin são gerados pelo CommonLibSSE-NG.
SKSE_PLUGIN_LOAD(const SKSE::LoadInterface* skse)
{
    // Inicializa o SKSE e prepara os callbacks de salvamento dos Actor Values.
    SKSE::Init(skse);
    IAFT::RegistrarSerializacaoAV();

    // Inicializa o log e anota a versao do Skyrim em execucao.
    IniciarLog();

    LogInfo(
        "Versão do Skyrim: " + skse->RuntimeVersion().string(),
        "Skyrim version: " + skse->RuntimeVersion().string()
    );

    // Obtem o sistema de mensagens usado para acompanhar o ciclo do jogo.
    auto* messaging = SKSE::GetMessagingInterface();

    if (!messaging) {
        return false;
    }

    // Reage ao carregamento de dados, inicio de jogo e troca de save.
    messaging->RegisterListener("SKSE", [](SKSE::MessagingInterface::Message* message)
    {
        if (!message) {
            return;
        }

        switch (message->type) {
        // Registra que o jogo terminou de carregar os dados principais.
        case SKSE::MessagingInterface::kDataLoaded:
            LogInfo(
                "Plugin carregado no menu principal",
                "Plugin loaded in main menu"
            );
            break;

        // Em novo jogo ou save carregado, registra eventos e inicia o combate.
        case SKSE::MessagingInterface::kNewGame:
        case SKSE::MessagingInterface::kPostLoadGame:
            LogInfo(
                "InvisibilityAffectsFollowersToo carregado",
                "InvisibilityAffectsFollowersToo loaded"
            );

            RegisterAnimationEvents();
            RegisterAllyEvents();
            IAFT::IniciarControleCombate(IsInvocacaoAliada);

            // Ativa os efeitos caso o save tenha sido carregado
            // com o jogador já agachado.
            if (auto* player = RE::PlayerCharacter::GetSingleton()) {
                const bool estaAgachado = player->IsSneaking();
                const bool estaInvisivel = JogadorEstaInvisivel();

                AtualizarFlagsInvisibilidade(estaInvisivel);

                if (estaAgachado || estaInvisivel) {
                    AtivarEfeitosSeguidores();
                }
            }

            break;

        // Antes de trocar de save, para o monitor e desfaz os efeitos ativos.
        case SKSE::MessagingInterface::kPreLoadGame:
            IAFT::PararControleCombate();
            RemoverFlagsInvisibilidade();
            DesativarEfeitosSeguidores();
            break;

        default:
            break;
        }
    });

    return true;
}