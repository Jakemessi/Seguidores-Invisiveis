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
    }
}

// =======================================================
// 1. EVENT HANDLER DE ANIMAÇÃO (DEBUG COMPLETO)
// =======================================================
class SneakEventHandler : public RE::BSTEventSink<RE::BSAnimationGraphEvent>
{
public:
    RE::BSEventNotifyControl ProcessEvent(
        const RE::BSAnimationGraphEvent* event,
        RE::BSTEventSource<RE::BSAnimationGraphEvent>*) override
    {
        if (!event || event->tag.empty()) {
            return RE::BSEventNotifyControl::kContinue;
        }

        std::string tag = event->tag.c_str();

        SeguidoresInvisiveis::Print(
            fmt::format("[SI Evento de Animação detectado] {}", tag)
        );

        return RE::BSEventNotifyControl::kContinue;
    }
};

static SneakEventHandler g_sneakHandler;

// =======================================================
// 2. REGISTRO DO SINK
// =======================================================
void RegisterAnimationSink()
{
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) {
        SeguidoresInvisiveis::Print("[SI ERRO] Player não encontrado");
        return;
    }

    // Evita duplicação em reloads
    player->RemoveAnimationGraphEventSink(&g_sneakHandler);

    player->AddAnimationGraphEventSink(&g_sneakHandler);

    SeguidoresInvisiveis::Print("[SI OK] AnimationGraphEventSink registrado no Player");
}

// =======================================================
// 3. MESSAGING HANDLER SKSE
// =======================================================
SKSEPluginLoad(const SKSE::LoadInterface* skse)
{
    SKSE::Init(skse);

    SKSE::GetMessagingInterface()->RegisterListener(
        [](SKSE::MessagingInterface::Message* message)
        {
            switch (message->type)
            {
                // Menu / inicialização básica
                case SKSE::MessagingInterface::kDataLoaded:
                {
                    SeguidoresInvisiveis::Print("[SI SKSE] DataLoaded (menu)");
                    break;
                }

                // Novo jogo
                case SKSE::MessagingInterface::kNewGame:
                {
                    SeguidoresInvisiveis::Print("[SI SKSE] NewGame detectado");
                    RegisterAnimationSink();
                    break;
                }

                // Save carregado
                case SKSE::MessagingInterface::kPostLoadGame:
                {
                    SeguidoresInvisiveis::Print("[SI SKSE] Save carregado (PostLoadGame)");
                    RegisterAnimationSink();
                    break;
                }

                default:
                    break;
            }
        });

    return true;
}