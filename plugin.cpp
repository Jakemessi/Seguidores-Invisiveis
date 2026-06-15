#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <functional>


void Print(const std::string& msg)
{
    if (auto* console = RE::ConsoleLog::GetSingleton()) {
        console->Print(msg.c_str());
    }
}


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

    ForEachFollower([](RE::Actor* actor)
    {
        actor->AsActorValueOwner()->ModActorValue(
            RE::ActorValue::kSneak,
            100.0f);


        Print(std::string(actor->GetName()) +
              " recebeu boost de sneak");
    });
}

void DisableFollowerStealth()
{
   
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

        if (tag == "tailSneakIdle") {
            Print("sneak detectado");
            EnableFollowerStealth();
        }

        /* Será feito depois, mas aqui seria o local para detectar o fim do sneak e chamar DisableFollowerStealth()
        else if (...)
        */
        return RE::BSEventNotifyControl::kContinue;
    }
};

void RegisterAnimationEvents()
{
    auto* player = RE::PlayerCharacter::GetSingleton();

    if (!player) {
        Print("Player não encontrado");
        return;
    }

    player->AddAnimationGraphEventSink(
        AnimationEventSink::GetSingleton());

    Print("AnimationGraphEvent registrado");
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

SKSEPluginLoad(const SKSE::LoadInterface* skse)
{
    SKSE::Init(skse);

    SKSE::GetMessagingInterface()->RegisterListener(
        [](SKSE::MessagingInterface::Message* message)
        {
            if (message->type == SKSE::MessagingInterface::kPostLoadGame) {
                Print("SeguidoresInvisiveis carregado");
                RegisterAnimationEvents();
            }
        });

    return true;
}
