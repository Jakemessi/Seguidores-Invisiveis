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
            if (!actor || actor->IsPlayerRef() || !actor->IsPlayerTeammate()) {
                continue;
            }

            followerCount++;
            const char* name = actor->GetName();

            Print(fmt::format(
                "[SeguidoresInvisiveis] Follower detectado: {} ({:08X})",
                name ? name : "<sem nome>",
                actor->GetFormID()));

            // Aqui você aplicará a lógica para torná-los indetectáveis futuramente
        }

        Print(fmt::format("[SeguidoresInvisiveis] Total de followers encontrados: {}", followerCount));
    }
}

class SneakEventHandler : public RE::BSTEventSink<RE::BSAnimationGraphEvent>
{
public:
    RE::BSEventNotifyControl ProcessEvent(
        const RE::BSAnimationGraphEvent* event,
        RE::BSTEventSource<RE::BSAnimationGraphEvent>*) override
    {
        if (!event) {
            return RE::BSEventNotifyControl::kContinue;
        }

        // Filtra para exibir apenas animações relevantes de agachamento, se desejar
        // Exemplo: se quiser apenas o início e o fim do sneak
        if (event->tag == "SneakStart" || event->tag == "SneakStop") {
            SeguidoresInvisiveis::Print(fmt::format("[StealthFollowers] EVENT RECEBIDO: {}", event->tag.c_str()));
        }

        return RE::BSEventNotifyControl::kContinue;
    }
};

static SneakEventHandler g_sneakHandler;

// Função separada para registrar o sink de animação com segurança
void RegisterAnimationSink()
{
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (player) {
        player->AddAnimationGraphEventSink(&g_sneakHandler);
        SeguidoresInvisiveis::Print("[SeguidoresInvisiveis] Animation handler registrado com sucesso no Player!");
    } else {
        SeguidoresInvisiveis::Print("[SeguidoresInvisiveis] ERRO: Player nao encontrado ao carregar o jogo!");
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* skse)
{
    SKSE::Init(skse);

    SKSE::GetMessagingInterface()->RegisterListener(
        [](SKSE::MessagingInterface::Message* message)
        {
            // kDataLoaded: Bom apenas para logs iniciais ou carregar texturas/UI
            if (message->type == SKSE::MessagingInterface::kDataLoaded) {
                SeguidoresInvisiveis::Print("[SeguidoresInvisiveis] Plugin carregado no Menu Principal!");
            }
            
            // kPostLoadGame: Disparado IMEDIATAMENTE após o jogador carregar um save file
            // kNewGame: Disparado se o jogador iniciar um novo jogo
            else if (message->type == SKSE::MessagingInterface::kPostLoadGame || 
                     message->type == SKSE::MessagingInterface::kNewGame) {
                
                SeguidoresInvisiveis::Print("[SeguidoresInvisiveis] Jogo carregado/iniciado! Registrando componentes...");
                RegisterAnimationSink();
            }
        });

    return true;
}
