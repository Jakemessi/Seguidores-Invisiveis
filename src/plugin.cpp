#include "PCH.h"
#include "Version.h"
#include "Logger.h"
// Estado do plugin
bool g_stealthAtivado = false;
constexpr bool p_debug = false;

// Valores aplicados pelo plugin
float g_sneakBoost = 1000.0f;
float g_invisibilityBoost = 1.0f;

// Camada do Actor Value usada pelo plugin.
// kTemporary indica que o boost deve ser tratado como modificador temporário,
// em vez de uma alteração permanente no valor do ator, assim sendo até mais fácil de remover.
constexpr RE::ACTOR_VALUE_MODIFIER g_modificadorActorValue = RE::ACTOR_VALUE_MODIFIER::kTemporary;

// std::vector é uma lista dinâmica: diferente de um array comum, ele pode crescer
// ou diminuir durante a execução. Aqui ele guarda vários ActorHandle, ou seja,
// referências seguras para os seguidores que receberam o efeito do plugin.
std::vector<RE::ActorHandle> g_seguidores;

std::string GetActorDisplayName(RE::Actor* actor)
{
    if (!actor) {
        return VaiBrasa ? "Ator desconhecido" : "Unknown actor";
    }

    const char* name = actor->GetName();

    if (!name || name[0] == '\0') {
        return VaiBrasa ? "Ator sem nome" : "Unnamed actor";
    }

    return std::string(name);
}

// ------------------------------------------------------------
// Busca de seguidores
// ------------------------------------------------------------

bool IsFollower(RE::Actor* actor)
{
    auto* player = RE::PlayerCharacter::GetSingleton();

    return actor &&
           actor != player &&
           actor->IsPlayerTeammate();
}

void ForEachFollower(const std::function<void(RE::Actor*, const RE::ActorHandle&)>& callback)
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

        if (!IsFollower(actor)) {
            continue;
        }

        callback(actor, handle);
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

    actor->ModActorValue(
        g_modificadorActorValue,
        RE::ActorValue::kSneak,
        g_sneakBoost
    );

    actor->ModActorValue(
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

    actor->ModActorValue(
        g_modificadorActorValue,
        RE::ActorValue::kSneak,
        -g_sneakBoost
    );

    actor->ModActorValue(
        g_modificadorActorValue,
        RE::ActorValue::kInvisibility,
        -g_invisibilityBoost
    );

    return true;
}

void AtivarEfeitosSeguidores()
{
    if (g_stealthAtivado) {
        return;
    }

    g_stealthAtivado = true;
    g_seguidores.clear();

    ForEachFollower([](RE::Actor* actor, const RE::ActorHandle& handle)
    {
        if (!AplicarEfeitos(actor)) {
            return;
        }

        g_seguidores.push_back(handle);

        const auto actorName = GetActorDisplayName(actor);

        LogInfo(
            actorName + " recebeu boost de sneak e invisibilidade",
            actorName + " received sneak boost and invisibility");
    });
}

void DesativarEfeitosSeguidores()
{
    if (!g_stealthAtivado) {
        return;
    }

    g_stealthAtivado = false;

    for (const auto& handle : g_seguidores) {
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
            actorName + " perdeu boost de sneak e invisibilidade",
            actorName + " lost sneak boost and invisibility");
    }

    g_seguidores.clear();
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

        if constexpr (p_debug){
            if (g_loggerIniciado) {
                const std::string tag = event->tag.c_str();
                LogInfo(
                "Evento de animacao recebido: " + tag,
                "Animation event received: " + tag);
            }
        }

        const bool estaAgachado = player->IsSneaking();

        if constexpr (p_debug) {
            if (g_loggerIniciado) {
                LogInfo(
                    "Estado do player: IsSneaking=" + std::string(estaAgachado ? "true" : "false") + ", g_stealthAtivado=" + std::string(g_stealthAtivado ? "true" : "false"),
                    "Player state: IsSneaking=" + std::string(estaAgachado ? "true" : "false") + ", g_stealthAtivado=" + std::string(g_stealthAtivado ? "true" : "false")
                );
            }
        }

        if (estaAgachado && !g_stealthAtivado) {
            LogInfo("Sneak detectado", "Sneak detected");
            AtivarEfeitosSeguidores();
        }
        else if (!estaAgachado && g_stealthAtivado) {
            LogInfo("Saindo de sneak", "Exiting sneak");
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
        "AnimationGraphEvent registered/re-registered");
}

// ------------------------------------------------------------
// Entrada principal do plugin
// ------------------------------------------------------------
// Do CommonlibsSE
extern "C" __declspec(dllexport) constinit auto SKSEPlugin_Version = []()
{
    SKSE::PluginVersionData v;

    v.PluginVersion(REL::Version{
        PLUGIN_VERSION_MAJOR,
        PLUGIN_VERSION_MINOR,
        PLUGIN_VERSION_PATCH,
        PLUGIN_VERSION_BUILD
    });
    v.PluginName(PLUGIN_NAME);
    v.AuthorName(PLUGIN_AUTHOR);
    v.UsesAddressLibrary();
    v.UsesNoStructs();
    v.CompatibleVersions({ SKSE::RUNTIME_SSE_LATEST });

    return v;
}();

extern "C" __declspec(dllexport) bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* skse)
{
    SKSE::Init(skse);

    IniciarLog();

    logger::info("Game version: {}", skse->RuntimeVersion().string());

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
                "Plugin loaded in main menu");
            break;

        case SKSE::MessagingInterface::kPostLoadGame:
            LogInfo(
                "InvisibilityAffectsFollowersToo carregado",
                "InvisibilityAffectsFollowersToo loaded");

            RegisterAnimationEvents();
            break;

        case SKSE::MessagingInterface::kPreLoadGame:
            DesativarEfeitosSeguidores();
            break;

        default:
            break;
        }
    });

    return true;
}