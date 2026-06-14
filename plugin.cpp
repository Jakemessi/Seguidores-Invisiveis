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

    // Função que aplica ou remove o estado furtivo absoluto nos seguidores
    void SetFollowersUndetectable(bool enable)
    {
        auto* processLists = RE::ProcessLists::GetSingleton();
        if (!processLists) {
            Print("[SI] ERRO: ProcessLists não encontrado");
            return;
        }

        std::uint32_t followerCount = 0;

        for (auto& handle : processLists->highActorHandles) {
            auto* actor = handle.get().get();
            if (!actor || actor->IsPlayerRef() || !actor->IsPlayerTeammate()) {
                continue;
            }

            followerCount++;
            const char* name = actor->GetName();

            if (enable) {
                // Modifica variáveis nativas do grafo de animação/IA para aplicar invisibilidade e silêncio absoluto
                actor->SetGraphVariableFloat("Invisibility", 1.0f);
                actor->SetGraphVariableFloat("Muffle", 1.0f);
                
                // Força inimigos que estavam mirando no seguidor a perderem o alvo imediatamente
                actor->StopCombat();

                Print(fmt::format("[SI] {} entrou em modo furtivo absoluto.", name ? name : "<sem nome>"));
            } else {
                // Restaura os valores originais do grafo do seguidor
                actor->SetGraphVariableFloat("Invisibility", 0.0f);
                actor->SetGraphVariableFloat("Muffle", 0.0f);

                Print(fmt::format("[SI] {} voltou ao estado normal.", name ? name : "<sem nome>"));
            }
        }
    }
}

// =======================================================
// 1. EVENT HANDLER DE ANIMAÇÃO
// =======================================================
class SneakEventHandler : public RE::BSTEventSink<RE::BSAnimationGraphEvent>
{
private:
    bool _isPlayerSneaking = false; // Controle interno para evitar disparos repetidos da mesma animação

public:
    RE::BSEventNotifyControl ProcessEvent(
        const RE::BSAnimationGraphEvent* event,
        RE::BSTEventSource<RE::BSAnimationGraphEvent>*) override
    {
        if (!event || event->tag.empty()) {
            return RE::BSEventNotifyControl::kContinue;
        }

        std::string tag = event->tag.c_str();

        // Detecta entrada no modo Sneak (Filtro universal para 'tailsneakidle', 'SneakStart', etc.)
        if (!_isPlayerSneaking && (tag.find("sneak") != std::string::npos)) {
            _isPlayerSneaking = true;
            SeguidoresInvisiveis::Print(fmt::format("[SI] Jogador agachou (Tag: {})", tag));
            SeguidoresInvisiveis::SetFollowersUndetectable(true);
        }
        // Detecta saída do modo Sneak (Filtro universal para 'tailmtidle', 'SneakStop', 'mtidle', 'walkStart')
        else if (_isPlayerSneaking && (tag.find("mtidle") != std::string::npos || tag.find("walk") != std::string::npos || tag == "SneakStop")) {
            _isPlayerSneaking = false;
            SeguidoresInvisiveis::Print(fmt::format("[SI] Jogador levantou (Tag: {})", tag));
            SeguidoresInvisiveis::SetFollowersUndetectable(false);
        }

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
                case SKSE::MessagingInterface::kDataLoaded:
                {
                    SeguidoresInvisiveis::Print("[SI SKSE] DataLoaded (menu)");
                    break;
                }

                case SKSE::MessagingInterface::kNewGame:
                {
                    SeguidoresInvisiveis::Print("[SI SKSE] NewGame detectado");
                    RegisterAnimationSink();
                    break;
                }

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
