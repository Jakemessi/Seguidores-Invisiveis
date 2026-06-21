#include <functional>
#include <SKSE/Logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
#include <filesystem>

//Para poder mudar log para português ou inglês
#include <Windows.h>
bool VaiBrasa = false;
// 

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
        actor->AsActorValueOwner()->ModActorValue(RE::ActorValue::kSneak,g_sneakBoost);
        if(VaiBrasa){
            //Print(std::string(actor->GetName()) + " recebeu boost de sneak");
            logger::info("{} recebeu boost de sneak", actor->GetName());
        } else {
            //Print(std::string(actor->GetName()) + " received sneak boost");
            logger::info("{} received sneak boost", actor->GetName());
        }
        
        /*Removido por entrar em conflito com sneak
        actor->AsActorValueOwner()->ModActorValue(RE::ActorValue::kMovementNoiseMult,-g_NoiseMod);
        if(VaiBrasa){
            //Print(std::string(actor->GetName()) + " não faz barulho");
            logger::info("{} não faz barulho", actor->GetName());
        } else {
            //Print(std::string(actor->GetName()) + " makes no noise");
            logger::info("{} makes no noise", actor->GetName());
        }
        */

        actor->AsActorValueOwner()->ModActorValue(RE::ActorValue::kInvisibility,1.0f);
        if(VaiBrasa){
            //Print(std::string(actor->GetName()) + " está invisível");
            logger::info("{} está invisível", actor->GetName());
        } else {
            //Print(std::string(actor->GetName()) + " is invisible");
            logger::info("{} is invisible", actor->GetName());
        }
        
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
        actor->AsActorValueOwner()->ModActorValue(RE::ActorValue::kSneak,-g_sneakBoost);

        if(VaiBrasa){
            //Print(std::string(actor->GetName()) + " perdeu o boost de sneak");
            logger::info("{} perdeu o boost de sneak", actor->GetName());
        } else {
            //Print(std::string(actor->GetName()) + " lost sneak boost");
            logger::info("{} lost sneak boost", actor->GetName());
        }

        /*Removido por entrar em conflito com sneak
        actor->AsActorValueOwner()->ModActorValue(RE::ActorValue::kMovementNoiseMult,g_NoiseMod);
        if(VaiBrasa){
            //Print(std::string(actor->GetName()) + " voltou a fazer barulho");
            logger::info("{} voltou a fazer barulho", actor->GetName());
        } else {
            //Print(std::string(actor->GetName()) + " makes noise again");
            logger::info("{} makes noise again", actor->GetName());
        }
        */
       
        actor->AsActorValueOwner()->ModActorValue(RE::ActorValue::kInvisibility,-1.0f);
        if(VaiBrasa){
            //Print(std::string(actor->GetName()) + " voltou a ser visível");
            logger::info("{} voltou a ser visível", actor->GetName());
        } else {
            //Print(std::string(actor->GetName()) + " is visible again");
            logger::info("{} is visible again", actor->GetName());
        }
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
            if(VaiBrasa){
                //Print("sneak detectado");
                logger::info("sneak detectado");
            } else {
                //Print("sneak detected");
                logger::info("sneak detected");
            }
            EnableFollowerStealth();
        }

        else if (tag == "tailMTIdle" && g_stealthEnabled) {
            if(VaiBrasa){
                //Print("saindo de sneak");
                logger::info("saindo de sneak");
            } else {
                //Print("exiting sneak");
                logger::info("exiting sneak");
            }
            DisableFollowerStealth();
        }
        return RE::BSEventNotifyControl::kContinue;
    }
};

void RegisterAnimationEvents()
{
    auto* player = RE::PlayerCharacter::GetSingleton();

    if (!player) {
        if(VaiBrasa) {
            //Print("Player não encontrado");
            logger::error("Player não encontrado");
        } else {
            //Print("Player not found");
            logger::error("Player not found");
        }
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
    wchar_t localeName[LOCALE_NAME_MAX_LENGTH]{};
    std::string locale_name = "unknown";

    if (GetUserDefaultLocaleName(localeName, LOCALE_NAME_MAX_LENGTH)) {

        std::wstring wlocale(localeName);

        int sizeNeeded = WideCharToMultiByte(
            CP_UTF8,
            0,
            wlocale.c_str(),
            -1,
            nullptr,
            0,
            nullptr,
            nullptr);

        if (sizeNeeded > 0) {
            locale_name.resize(sizeNeeded - 1);

            WideCharToMultiByte(
                CP_UTF8,
                0,
                wlocale.c_str(),
                -1,
                locale_name.data(),
                sizeNeeded,
                nullptr,
                nullptr);

            VaiBrasa = locale_name.starts_with("pt");
        }
    }
    else {
        logger::error("GetUserDefaultLocaleName failed");
    }

    auto path = logger::log_directory();

    if (!path) {
        return;
    }

    *path /= "InvisibilityAffectsFollowersToo.log";

    auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
        path->string(), true);

    auto log = std::make_shared<spdlog::logger>(
        "global log",
        std::move(sink));

    log->set_level(spdlog::level::debug);
    log->flush_on(spdlog::level::info);

    spdlog::set_default_logger(std::move(log));
    spdlog::set_pattern("[%H:%M:%S] [%l] %v");

    logger::info("Logger Made In Arstotzka");

    if (VaiBrasa) {
        logger::info("Logger inicializado");
        logger::info("Idioma detectado: {}", locale_name);
    } else {
        logger::info("Logger initialized");
        logger::info("Detected language: {}", locale_name);
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* skse)
{
    
    SKSE::Init(skse);
    
    SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* message)
        {
            if (message->type == SKSE::MessagingInterface::kDataLoaded) {
                InitializeLogging();
                if(VaiBrasa){
                    //Print("Tentou iniciar Logger");
                    //Print("Plugin carregado no menu principal");
                    logger::info("Plugin carregado no menu principal");
                } else {
                    //Print("Tried to initialize Logger");
                    //Print("Plugin loaded in main menu");
                    logger::info("Plugin loaded in main menu");
                }
            }
            if (message->type == SKSE::MessagingInterface::kPostLoadGame) {
                if(VaiBrasa){
                    //Print("InvisibilityAffectsFollowersToo carregado");
                    logger::info("InvisibilityAffectsFollowersToo carregado");
                } else {
                    //Print("InvisibilityAffectsFollowersToo loaded");
                    logger::info("InvisibilityAffectsFollowersToo loaded");
                }
                RegisterAnimationEvents();
            }
        });

    return true;
}