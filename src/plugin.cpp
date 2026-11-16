#include "PCH.h"
#include "Version.h"
#include "Logger.h"

#include <algorithm>

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

    struct AliadoFlagStealth
    {
        RE::ActorHandle handle;
        bool jaNaoApareciaNoStealthMeter = false;
    };

    std::vector<AliadoFlagStealth> g_flagsStealth;

    std::string GetActorDisplayName(RE::Actor* actor)
    {
        if (!actor) {
            return VaiBrasa() ? "Ator desconhecido" : "Unknown actor";
        }

        const char* name = actor->GetName();

        if (!name || name[0] == '\0') {
            return VaiBrasa() ? "Ator sem nome" : "Unnamed actor";
        }

        return std::string(name);
    }


    // ------------------------------------------------------------
    // Verificar se o jogador está invisivel
    // ------------------------------------------------------------

    bool JogadorEstaInvisivel()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();

        if (!player) {
            return false;
        }

        auto* magicTarget = player->AsMagicTarget();

        return magicTarget &&
            magicTarget->HasEffectWithArchetype(
                RE::EffectArchetype::kInvisibility
            );
    }

    // ------------------------------------------------------------
    // Busca de seguidores
    // ------------------------------------------------------------

    bool IsFollower(RE::Actor* actor)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        return actor && actor != player && actor->IsPlayerTeammate();
    }

    // Invocações, comandados e reanimados pertencentes ao jogador ou a seguidores.
    // Um NPC amigável sem essa relação não entra no filtro.
    bool IsInvocacaoAliada(RE::Actor* actor)
    {
        if (!actor || (!actor->IsSummoned() && !actor->IsCommandedActor())) {
            return false;
        }

        auto comandante = actor->GetCommandingActor();
        auto* dono = comandante.get();
        auto* player = RE::PlayerCharacter::GetSingleton();

        return dono && dono != actor &&
            (dono == player || IsFollower(dono));
    }

    bool IsAliado(RE::Actor* actor)
    {
        return IsFollower(actor) || IsInvocacaoAliada(actor);
    }

    void ForEachAliado(const std::function<void(RE::Actor*, const RE::ActorHandle&)>& callback)
    {
        auto* processLists = RE::ProcessLists::GetSingleton();
        if (!processLists) {
            return;
        }

        for (const auto& handle : processLists->highActorHandles) {
            auto actorPtr = handle.get();
            if (!actorPtr) {
                continue;
            }

            auto* actor = actorPtr.get();
            if (IsAliado(actor)) {
                callback(actor, handle);
            }
        }
    }


    // ------------------------------------------------------------
    // Aplicação e remoção dos efeitos
    // ------------------------------------------------------------
    // Métodos de aplicação e remoção de AVs Temporários
    bool AplicarEfeitos(RE::Actor* actor)
    {
        if (!actor) {
            return false;
        }

        auto* actorValueOwner = actor->AsActorValueOwner();

        if (!actorValueOwner) {
            return false;
        }

        actorValueOwner->ModActorValue(
            g_modificadorActorValue,
            RE::ActorValue::kSneak,
            g_sneakBoost
        );

        actorValueOwner->ModActorValue(
            g_modificadorActorValue,
            RE::ActorValue::kInvisibility,
            g_invisibilityBoost
        );

        return true;
    }

    bool RemoverEfeitos(RE::Actor* actor)
    {
        if (!actor) {
            return false;
        }

        auto* actorValueOwner = actor->AsActorValueOwner();

        if (!actorValueOwner) {
            return false;
        }

        actorValueOwner->ModActorValue(
            g_modificadorActorValue,
            RE::ActorValue::kSneak,
            -g_sneakBoost
        );

        actorValueOwner->ModActorValue(
            g_modificadorActorValue,
            RE::ActorValue::kInvisibility,
            -g_invisibilityBoost
        );

        return true;
    }

    bool AliadoJaRegistrado(const RE::ActorHandle& handle)
    {
        return std::find(g_aliados.begin(), g_aliados.end(), handle) != g_aliados.end();
    }

    bool FlagJaRegistrada(const RE::ActorHandle& handle)
    {
        return std::find_if(
            g_flagsStealth.begin(), g_flagsStealth.end(),
            [&handle](const AliadoFlagStealth& registro) {
                return registro.handle == handle;
            }
        ) != g_flagsStealth.end();
    }

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

        if (!jaNaoAparecia) {
            flags.set(flag);
        }

        const auto nome = GetActorDisplayName(actor);
        LogInfo(nome + " nao afeta mais o stealth meter",
                nome + " no longer affects the stealth meter");
    }

    void SincronizarFlagsInvisibilidade()
    {
        constexpr auto flag = RE::Actor::BOOL_FLAGS::kDoNotShowOnStealthMeter;

        // Uma reanimação pode perder o comandante antes do fim da invisibilidade.
        for (auto it = g_flagsStealth.begin(); it != g_flagsStealth.end();) {
            auto actorPtr = it->handle.get();
            if (actorPtr && !IsAliado(actorPtr.get())) {
                if (!it->jaNaoApareciaNoStealthMeter) {
                    actorPtr->GetActorRuntimeData().boolFlags.reset(flag);
                }
                it = g_flagsStealth.erase(it);
            } else {
                ++it;
            }
        }

        ForEachAliado([](RE::Actor* actor, const RE::ActorHandle& handle) {
            AplicarFlagNaoAfetaStealth(actor, handle);
        });
    }

    void RemoverFlagsInvisibilidade()
    {
        if (!g_flagInvisibilidadeAtiva && g_flagsStealth.empty()) {
            return;
        }

        constexpr auto flag = RE::Actor::BOOL_FLAGS::kDoNotShowOnStealthMeter;
        for (const auto& registro : g_flagsStealth) {
            auto actorPtr = registro.handle.get();
            if (actorPtr && !registro.jaNaoApareciaNoStealthMeter) {
                actorPtr->GetActorRuntimeData().boolFlags.reset(flag);
            }
        }

        g_flagsStealth.clear();
        g_flagInvisibilidadeAtiva = false;

        LogInfo("Flags de stealth dos aliados restauradas",
                "Ally stealth flags restored");
    }

    void AtualizarFlagsInvisibilidade(bool estaInvisivel)
    {
        if (estaInvisivel) {
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
            RemoverFlagsInvisibilidade();
        }
    }

    void SincronizarSeguidores()
    {
        // Descarta comandados que deixaram de pertencer ao jogador/grupo.
        for (auto it = g_aliados.begin(); it != g_aliados.end();) {
            auto actorPtr = it->get();
            if (actorPtr && !IsAliado(actorPtr.get())) {
                RemoverEfeitos(actorPtr.get());
                it = g_aliados.erase(it);
            } else {
                ++it;
            }
        }

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

    void AtivarEfeitosSeguidores()
    {
        if (g_stealthAtivado) {
            return;
        }

        g_stealthAtivado = true;
        g_aliados.clear();

        SincronizarSeguidores();
    }

    void DesativarEfeitosSeguidores()
    {
        if (!g_stealthAtivado) {
            return;
        }

        g_stealthAtivado = false;

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

    class AllyEventSink :
        public RE::BSTEventSink<RE::TESObjectLoadedEvent>,
        public RE::BSTEventSink<RE::TESMagicEffectApplyEvent>
    {
    public:
        static AllyEventSink* GetSingleton()
        {
            static AllyEventSink singleton;
            return std::addressof(singleton);
        }

        RE::BSEventNotifyControl ProcessEvent(
            const RE::TESObjectLoadedEvent* event,
            RE::BSTEventSource<RE::TESObjectLoadedEvent>*) override
        {
            if (!event || !event->loaded ||
                (!g_stealthAtivado && !g_flagInvisibilidadeAtiva)) {
                return RE::BSEventNotifyControl::kContinue;
            }

            auto* actor = RE::TESForm::LookupByID<RE::Actor>(event->formID);
            if (IsAliado(actor)) {
                SincronizarAliadosAtivos();
            }
            return RE::BSEventNotifyControl::kContinue;
        }

        RE::BSEventNotifyControl ProcessEvent(
            const RE::TESMagicEffectApplyEvent* event,
            RE::BSTEventSource<RE::TESMagicEffectApplyEvent>*) override
        {
            if (!event || (!g_stealthAtivado && !g_flagInvisibilidadeAtiva)) {
                return RE::BSEventNotifyControl::kContinue;
            }

            auto* caster = event->caster ? event->caster->As<RE::Actor>() : nullptr;
            auto* target = event->target ? event->target->As<RE::Actor>() : nullptr;
            auto* player = RE::PlayerCharacter::GetSingleton();

            if (IsAliado(target) || caster == player || IsFollower(caster)) {
                SincronizarAliadosAtivos();
            }

            return RE::BSEventNotifyControl::kContinue;
        }
    };

    void RegisterAllyEvents()
    {
        auto* sources = RE::ScriptEventSourceHolder::GetSingleton();
        if (!sources) {
            LogErro("Fontes de evento indisponiveis", "Event sources unavailable");
            return;
        }

        sources->RemoveEventSink<RE::TESObjectLoadedEvent>(AllyEventSink::GetSingleton());
        sources->RemoveEventSink<RE::TESMagicEffectApplyEvent>(AllyEventSink::GetSingleton());
        sources->AddEventSink<RE::TESObjectLoadedEvent>(AllyEventSink::GetSingleton());
        sources->AddEventSink<RE::TESMagicEffectApplyEvent>(AllyEventSink::GetSingleton());
    }

    // ------------------------------------------------------------
    // Eventos de animação
    // ------------------------------------------------------------
    class AnimationEventSink :
        public RE::BSTEventSink<RE::BSAnimationGraphEvent>
    {
    public:
        static AnimationEventSink* GetSingleton()
        {
            static AnimationEventSink singleton;
            return std::addressof(singleton);
        }
        
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

            if constexpr (p_debug) {
                const std::string tag = event->tag.c_str();

                LogInfo(
                    "Evento de animacao recebido: " + tag,
                    "Animation event received: " + tag
                );
            }

            const bool estaAgachado = player->IsSneaking();
            const bool estaInvisivel = JogadorEstaInvisivel();

            AtualizarFlagsInvisibilidade(estaInvisivel);

            const bool EstaEmStealth = estaAgachado || estaInvisivel;

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

            if(EstaEmStealth) {
                if (!g_stealthAtivado) {

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
                LogInfo(
                    "Saindo de stealth",
                    "Exiting stealth"
                );

                DesativarEfeitosSeguidores();
            }

            return RE::BSEventNotifyControl::kContinue;
        }
    };

    void RegisterAnimationEvents()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();

        if (!player) {
            LogErro("Player não encontrado", "Player not found");
            return;
        }

        player->RemoveAnimationGraphEventSink(AnimationEventSink::GetSingleton());
        player->AddAnimationGraphEventSink(AnimationEventSink::GetSingleton());

        LogInfo(
            "AnimationGraphEvent registrado/re-registrado",
            "AnimationGraphEvent registered/re-registered"
        );
    }
}

// Entrada carregada pelo SKSE; metadados do plugin são gerados pelo CommonLibSSE-NG.
extern "C" __declspec(dllexport) bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* skse)
{
    SKSE::Init(skse);

    IniciarLog();

    LogInfo(
        "Versão do Skyrim: " + skse->RuntimeVersion().string(),
        "Skyrim version: " + skse->RuntimeVersion().string()
    );

    auto* messaging = SKSE::GetMessagingInterface();

    if (!messaging) {
        return false;
    }

    messaging->RegisterListener("SKSE", [](SKSE::MessagingInterface::Message* message)
    {
        if (!message) {
            return;
        }

        switch (message->type) {
        case SKSE::MessagingInterface::kDataLoaded:
            LogInfo(
                "Plugin carregado no menu principal",
                "Plugin loaded in main menu"
            );
            break;

        case SKSE::MessagingInterface::kPostLoadGame:
            LogInfo(
                "InvisibilityAffectsFollowersToo carregado",
                "InvisibilityAffectsFollowersToo loaded"
            );

            RegisterAnimationEvents();
            RegisterAllyEvents();

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

        case SKSE::MessagingInterface::kPreLoadGame:
            RemoverFlagsInvisibilidade();
            DesativarEfeitosSeguidores();
            break;

        default:
            break;
        }
    });

    return true;
}