#include <functional>
#include <SKSE/Logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
#include <filesystem>

//Para printar no console
void Print(const std::string& msg)
{
    if (auto* console = RE::ConsoleLog::GetSingleton()) {
        console->Print(msg.c_str());
    }
}

bool g_stealthEnabled = false;
float g_sneakBoost = 1000.0f;
float g_NoiseMod = 1.0f;

//Busca os seguidores
bool IsFollower(RE::Actor* actor)
{
    auto* player = RE::PlayerCharacter::GetSingleton();

    return actor &&
           actor != player &&
           actor->IsPlayerTeammate();
}

void ForEachFollower(const std::function<void(RE::Actor*)>& callback)
{
    auto* processLists = RE::ProcessLists::GetSingleton();

    if (!processLists) {
        return;
    }

    for (const auto& handle : processLists->highActorHandles) {
        auto actor = handle.get().get();

        if (!IsFollower(actor)) {
            continue;
        }

        callback(actor);
    }
}

void EnableFollowerStealth()
{
    if (g_stealthEnabled) {
        return;
    }

    g_stealthEnabled = true;

    ForEachFollower([](RE::Actor* actor)
    {
        actor->AsActorValueOwner()->ModActorValue(
            RE::ActorValue::kSneak,
            g_sneakBoost);

        //Print(std::string(actor->GetName()) +" recebeu boost de sneak");
        logger::info("{} recebeu boost de sneak", actor->GetName());
        /*Removido por entrar em conflito com sneak
        actor->AsActorValueOwner()->ModActorValue(
            RE::ActorValue::kMovementNoiseMult,
            -g_NoiseMod);

        //Print(std::string(actor->GetName()) + " não faz barulho");
        logger::info("{} não faz barulho", actor->GetName());
        */
        actor->AsActorValueOwner()->ModActorValue(
            RE::ActorValue::kInvisibility,
            1.0f);

        //Print(std::string(actor->GetName()) + " está invisível");
        logger::info("{} está invisível", actor->GetName());
        
    });
}

void DisableFollowerStealth()
{
    if (!g_stealthEnabled) {
        return;
    }

    g_stealthEnabled = false;

    ForEachFollower([](RE::Actor* actor)
    {
        actor->AsActorValueOwner()->ModActorValue(
            RE::ActorValue::kSneak,
            -g_sneakBoost);

        //Print(std::string(actor->GetName()) + " perdeu o boost de sneak");
        logger::info("{} perdeu o boost de sneak", actor->GetName());

        /*Removido por entrar em conflito com sneak
        actor->AsActorValueOwner()->ModActorValue(
            RE::ActorValue::kMovementNoiseMult,
            g_NoiseMod);
        //Print(std::string(actor->GetName()) + " voltou a fazer barulho");
        logger::info("{} voltou a fazer barulho", actor->GetName());
        //
        */
       
        actor->AsActorValueOwner()->ModActorValue(
            RE::ActorValue::kInvisibility,
            -1.0f);

        //Print(std::string(actor->GetName()) + " voltou a ser visível");
        logger::info("{} voltou a ser visível", actor->GetName());
        
    });
}

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

        const std::string tag = event->tag.c_str();

        if (tag == "tailSneakIdle" && !g_stealthEnabled) {
            //Print("sneak detectado");
            logger::info("sneak detectado");
            EnableFollowerStealth();
        }

        else if (tag == "tailMTIdle" && g_stealthEnabled) {
            //Print("saindo de sneak");
            logger::info("saindo de sneak");
            DisableFollowerStealth();
        }
        return RE::BSEventNotifyControl::kContinue;
    }
};

void RegisterAnimationEvents()
{
    auto* player = RE::PlayerCharacter::GetSingleton();

    if (!player) {
        //Print("Player não encontrado");
        logger::error("Player não encontrado");
        return;
    }

    player->AddAnimationGraphEventSink(
        AnimationEventSink::GetSingleton());

    //Print("AnimationGraphEvent registrado");
    logger::info("AnimationGraphEvent registrado");
    
}

//Tentativa de aplicar a flag
/*
void SetFollowerStealth(RE::Actor* actor, bool enable)
{
    if (!actor) {
        return;
    }

    if (enable) {
        actor->boolFlags.set(RE::Actor::BOOL_FLAGS::kDoNotShowOnStealthMeter);
    } else {
        actor->boolFlags.reset(RE::Actor::BOOL_FLAGS::kDoNotShowOnStealthMeter);
    }
}
*/

//Para a criação de logs
void InitializeLogging()
{
    //Print("Inicializando logger");
    auto path = logger::log_directory();

    if (!path) {
        Print("logger::log_directory() retornou nullptr");
        return;
    }

    //Print(path->string());
	
    *path /= "InvisibilityAffectsFollowersToo.log";

    auto sink =
        std::make_shared<spdlog::sinks::basic_file_sink_mt>(
            path->string(), true);

    auto log =
        std::make_shared<spdlog::logger>(
            "global log",
            std::move(sink));

    log->set_level(spdlog::level::debug);
    log->flush_on(spdlog::level::info);

    spdlog::set_default_logger(std::move(log));
    spdlog::set_pattern("[%H:%M:%S] [%l] %v");
    logger::info("Logger Made In Arstotzka");
    logger::info("Logger inicializado");
}

SKSEPluginLoad(const SKSE::LoadInterface* skse)
{
    
    SKSE::Init(skse);
    
    SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* message)
        {
            if (message->type == SKSE::MessagingInterface::kDataLoaded) {
                InitializeLogging();
                //Print("Tentou iniciar Logger");
                //Print("Plugin carregado no menu principal");
                logger::info("Plugin carregado no menu principal");
            }
            if (message->type == SKSE::MessagingInterface::kPostLoadGame) {
                //Print("InvisibilityAffectsFollowersToo carregado");
                logger::info("InvisibilityAffectsFollowersToo carregado");
                RegisterAnimationEvents();
            }
        });

    return true;
}