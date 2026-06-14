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
            Print("[SI] ERRO: ProcessLists nao encontrado");
            return;
        }

        for (auto& handle : processLists->highActorHandles) {
            auto* actor = handle.get().get();
            if (!actor || actor->IsPlayerRef() || !actor->IsPlayerTeammate()) {
                continue;
            }

            const char* name = actor->GetName();

            if (enable) {
                actor->SetGraphVariableFloat("Invisibility", 1.0f);
                actor->SetGraphVariableFloat("Muffle", 1.0f);
                actor->StopCombat();
                Print(fmt::format("[SI] {} entrou em modo furtivo absoluto.", name ? name : "<sem nome>"));
            } else {
                actor->SetGraphVariableFloat("Invisibility", 0.0f);
                actor->SetGraphVariableFloat("Muffle", 0.0f);
                Print(fmt::format("[SI] {} voltou ao estado normal.", name ? name : "<sem nome>"));
            }
        }
    }
}

// =======================================================
// 1. EVENT HANDLER DE INPUT (DIPARA QUANDO SNEAK MUDA)
// =======================================================
class InputEventHandler : public RE::BSTEventSink<RE::InputEvent*>
{
private:
    bool _isPlayerSneaking = false;

public:
        RE::BSEventNotifyControl ProcessEvent(
        RE::InputEvent* const* eventPtr,
        RE::BSTEventSource<RE::InputEvent*>*) override
    {
        if (!eventPtr) return RE::BSEventNotifyControl::kContinue;

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || !RE::UI::GetSingleton() || RE::UI::GetSingleton()->GameIsPaused()) {
            return RE::BSEventNotifyControl::kContinue;
        }

        for (auto* event = *eventPtr; event; event = event->next) {
            // Chamamos HasIDCode() diretamente do 'event' (InputEvent)
            if (event->GetEventType() != RE::INPUT_EVENT_TYPE::kButton || !event->HasIDCode()) {
                continue;
            }

            auto* buttonEvent = event->AsButtonEvent();
            if (!buttonEvent) {
                continue;
            }

            // Captura o nome da acao associada ao botao
            const auto& userEvent = buttonEvent->QUserEvent();

            // "Sneak" e o ID nativo do controle/teclado para agachar no Skyrim
            if (userEvent == "Sneak" && buttonEvent->IsDown()) {
                
                // O estado do jogo atualiza logo apos o input, verificamos o inverso atual
                bool currentlySneaking = player->IsSneaking();
                
                // Se não estava agachado, significa que agora vai agachar
                if (!currentlySneaking && !_isPlayerSneaking) {
                    _isPlayerSneaking = true;
                    SeguidoresInvisiveis::Print("[SI] Jogador AGACHOU.");
                    SeguidoresInvisiveis::SetFollowersUndetectable(true);
                } 
                // Se ja estava agachado, significa que agora vai levantar
                else if (currentlySneaking && _isPlayerSneaking) {
                    _isPlayerSneaking = false;
                    SeguidoresInvisiveis::Print("[SI] Jogador LEVANTOU.");
                    SeguidoresInvisiveis::SetFollowersUndetectable(false);
                }
            }
        }

        return RE::BSEventNotifyControl::kContinue;
    }
};

static InputEventHandler g_inputHandler;

// =======================================================
// 2. REGISTRO SEGURO DO HANDLER DE INPUT
// =======================================================
void RegisterInputSink()
{
    auto* inputManager = RE::BSInputDeviceManager::GetSingleton();
    if (inputManager) {
        inputManager->AddEventSink(&g_inputHandler);
        SeguidoresInvisiveis::Print("[SI OK] Rastreamento de botoes ativado com sucesso!");
    } else {
        SeguidoresInvisiveis::Print("[SI ERRO] Falha ao acessar gerenciador de input.");
    }
}

// =======================================================
// 3. MESSAGING HANDLER SKSE
// =======================================================
void MessageHandler(SKSE::MessagingInterface::Message* message)
{
    switch (message->type)
    {
        case SKSE::MessagingInterface::kDataLoaded:
            SeguidoresInvisiveis::Print("[SI SKSE] Mod carregado. Iniciando escuta de botoes...");
            RegisterInputSink(); // Registra uma unica vez aqui, o InputManager e global e permanente
            break;

        default:
            break;
    }
}

// Ponto de entrada padrao do SKSE
extern "C" [[nodiscard]] __declspec(dllexport) bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* skse)
{
    SKSE::Init(skse);

    auto* messaging = SKSE::GetMessagingInterface();
    if (messaging) {
        messaging->RegisterListener(MessageHandler);
    }

    return true;
}
