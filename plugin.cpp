#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <fmt/format.h>
#include <string>

namespace SeguidoresInvisiveis
{
    void Print(const std::string& message)
    {
        if (auto* console = RE::ConsoleLog::GetSingleton()) {
            console->Print(message.c_str());
        }

        // logger::info("{}", message);
    }

    void SetFollowersUndetectable(bool enable)
    {
        auto* processLists = RE::ProcessLists::GetSingleton();

        if (!processLists) {
            Print("[SeguidoresInvisiveis] ERRO: ProcessLists nao encontrado");
            return;
        }

        std::uint32_t followerCount = 0;

        for (auto& handle : processLists->highActorHandles) {
            auto* actor = handle.get().get();

            if (!actor) {
                continue;
            }

            if (actor->IsPlayerRef()) {
                continue;
            }

            if (!actor->IsPlayerTeammate()) {
                continue;
            }

            followerCount++;

            const char* name = actor->GetName();

            Print(fmt::format(
                "[SeguidoresInvisiveis] Follower detectado: {} ({:08X})",
                name ? name : "<sem nome>",
                actor->GetFormID()));
        }

        Print(fmt::format(
            "[SeguidoresInvisiveis] Total de followers encontrados: {}",
            followerCount));
    }
}

class SneakEventHandler :
    public RE::BSTEventSink<RE::BSAnimationGraphEvent>
{
public:
    RE::BSEventNotifyControl ProcessEvent(
        const RE::BSAnimationGraphEvent* event,
        RE::BSTEventSource<RE::BSAnimationGraphEvent>*)
        override
    {
        if (!event) {
            return RE::BSEventNotifyControl::kContinue;
        }

        auto* player = RE::PlayerCharacter::GetSingleton();

        if (event->holder != player) {
            return RE::BSEventNotifyControl::kContinue;
        }

        SeguidoresInvisiveis::Print(
            fmt::format(
                "[SeguidoresInvisiveis] Anim Event: {}",
                event->tag.c_str()));

        return RE::BSEventNotifyControl::kContinue;
    }
};

static SneakEventHandler g_sneakHandler;

SKSEPluginLoad(const SKSE::LoadInterface* skse)
{
    SKSE::Init(skse);

    SKSE::GetMessagingInterface()->RegisterListener(
        [](SKSE::MessagingInterface::Message* message)
        {
            if (message->type == SKSE::MessagingInterface::kDataLoaded) {

                auto* console = RE::ConsoleLog::GetSingleton();

                if (console) {
                    console->Print("[SeguidoresInvisiveis] Plugin carregado!");
                }

                auto* player = RE::PlayerCharacter::GetSingleton();

                if (player) {

                    console->Print("[SeguidoresInvisiveis] Player encontrado!");

                    player->AddAnimationGraphEventSink(&g_sneakHandler);

                    console->Print(
                        "[SeguidoresInvisiveis] Animation handler registrado!");
                }
                else {
                    console->Print(
                        "[SeguidoresInvisiveis] Player NAO encontrado!");
                }
            }
        });

    return true;
}