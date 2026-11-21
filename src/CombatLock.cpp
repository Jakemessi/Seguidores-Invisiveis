#include "PCH.h"
#include "CombatLock.h"
#include "Logger.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <thread>

namespace IAFT
{
    namespace
    {
        constexpr auto kIntervaloMonitor = std::chrono::milliseconds(400);
        constexpr auto kAtaqueDesabilitado = RE::Actor::BOOL_FLAGS::kAttackingDisabled;
        constexpr auto kConjuracaoDesabilitada = RE::Actor::BOOL_FLAGS::kCastingDisabled;

        struct Bloqueio
        {
            RE::ActorHandle handle;
            bool ataqueJaDesabilitado = false;
            bool conjuracaoJaDesabilitada = false;
        };

        std::vector<Bloqueio> g_bloqueios;
        std::function<bool(RE::Actor*)> g_eInvocacaoAliada;
        bool g_combateLiberado = false;

        // O monitor apenas agenda tarefas. Todos os acessos a atores/flags
        // acontecem nas tarefas da thread principal do Skyrim.
        std::jthread g_monitor;
        std::atomic<std::uint64_t> g_geracao{ 0 };
        std::atomic<bool> g_tarefaPendente{ false };

        bool EInvocacaoAliada(RE::Actor* actor)
        {
            return actor && !actor->IsDead() &&
                   g_eInvocacaoAliada && g_eInvocacaoAliada(actor);
        }

        void RestaurarAtaques(const Bloqueio& bloqueio)
        {
            auto ator = bloqueio.handle.get();
            if (!ator) {
                return;
            }

            auto& flags = ator->GetActorRuntimeData().boolFlags;
            if (!bloqueio.ataqueJaDesabilitado) {
                flags.reset(kAtaqueDesabilitado);
            }
            if (!bloqueio.conjuracaoJaDesabilitada) {
                flags.reset(kConjuracaoDesabilitada);
            }
        }

        void RestaurarTodos()
        {
            for (const auto& bloqueio : g_bloqueios) {
                RestaurarAtaques(bloqueio);
            }
            g_bloqueios.clear();
        }

        bool JaBloqueado(const RE::ActorHandle& handle)
        {
            return std::any_of(g_bloqueios.begin(), g_bloqueios.end(),
                [&handle](const Bloqueio& bloqueio) {
                    return bloqueio.handle == handle;
                });
        }

        void SincronizarBloqueios()
        {
            // Se um morto reanimado perder o comandante ou morrer,
            // os flags que adicionamos sao restaurados.
            for (auto it = g_bloqueios.begin(); it != g_bloqueios.end();) {
                auto ator = it->handle.get();

                if (!ator || !EInvocacaoAliada(ator.get())) {
                    RestaurarAtaques(*it);
                    it = g_bloqueios.erase(it);
                } else {
                    ++it;
                }
            }

            auto* listas = RE::ProcessLists::GetSingleton();
            if (!listas) {
                return;
            }

            for (const auto& handle : listas->highActorHandles) {
                auto ator = handle.get();
                if (!ator || !EInvocacaoAliada(ator.get()) || JaBloqueado(handle)) {
                    continue;
                }

                auto& flags = ator->GetActorRuntimeData().boolFlags;
                g_bloqueios.push_back({
                    handle,
                    flags.all(kAtaqueDesabilitado),
                    flags.all(kConjuracaoDesabilitada)
                });

                flags.set(kAtaqueDesabilitado);
                flags.set(kConjuracaoDesabilitada);
            }
        }

        bool EInimigoValido(RE::Actor* actor, RE::Actor* player)
        {
            return actor && actor != player && !actor->IsDead() &&
                   !actor->IsPlayerTeammate() && !EInvocacaoAliada(actor) &&
                   actor->IsHostileToActor(player);
        }

        bool JogadorDetectadoPorInimigo(RE::Actor* player)
        {
            auto* listas = RE::ProcessLists::GetSingleton();
            if (!listas || !player) {
                return false;
            }

            for (const auto& handle : listas->highActorHandles) {
                auto ator = handle.get();
                if (ator && EInimigoValido(ator.get(), player) &&
                    ator->RequestDetectionLevel(player) > 0) {
                    return true;
                }
            }
            return false;
        }

        bool HaInimigosCombatendo(RE::Actor* player)
        {
            auto* listas = RE::ProcessLists::GetSingleton();
            if (!listas || !player) {
                return false;
            }

            // Mantem a liberacao enquanto houver hostis proximos em combate,
            // mesmo quando o jogador ja saiu de combate ou voltou a se ocultar.
            for (const auto& handle : listas->highActorHandles) {
                auto ator = handle.get();
                if (ator && EInimigoValido(ator.get(), player) && ator->IsInCombat()) {
                    return true;
                }
            }
            return false;
        }
    }

    void AtualizarControleCombate()
    {
        if (!g_eInvocacaoAliada) {
            return;
        }

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            return;
        }

        // So a primeira deteccao ou a entrada em combate libera os ataques.
        // Depois, o combate permanece liberado ate o fim do confronto.
        const bool gatilho = player->IsInCombat() || JogadorDetectadoPorInimigo(player);

        if (gatilho && !g_combateLiberado) {
            g_combateLiberado = true;
            RestaurarTodos();
            LogInfo("Combate liberado para invocacoes aliadas",
                    "Combat enabled for allied summons");
        } else if (!gatilho && g_combateLiberado && !HaInimigosCombatendo(player)) {
            g_combateLiberado = false;
            LogInfo("Combate bloqueado para invocacoes aliadas",
                    "Combat disabled for allied summons");
        }

        if (g_combateLiberado) {
            RestaurarTodos();
        } else {
            SincronizarBloqueios();
        }
    }

    void PararControleCombate()
    {
        // Invalida as tarefas pendentes de uma partida anterior.
        ++g_geracao;
        if (g_monitor.joinable()) {
            g_monitor.request_stop();
            g_monitor.join();
        }

        g_tarefaPendente.store(false);
        RestaurarTodos();
        g_combateLiberado = false;
        g_eInvocacaoAliada = {};
    }

    void IniciarControleCombate(std::function<bool(RE::Actor*)> a_eInvocacaoAliada)
    {
        PararControleCombate();
        g_eInvocacaoAliada = std::move(a_eInvocacaoAliada);

        const auto sessao = ++g_geracao;
        AtualizarControleCombate();

        g_monitor = std::jthread([sessao](std::stop_token stop) {
            while (!stop.stop_requested()) {
                std::this_thread::sleep_for(kIntervaloMonitor);
                if (stop.stop_requested() || sessao != g_geracao.load()) {
                    break;
                }

                if (g_tarefaPendente.exchange(true)) {
                    continue;
                }

                auto* tarefas = SKSE::GetTaskInterface();
                if (!tarefas) {
                    g_tarefaPendente.store(false);
                    continue;
                }

                tarefas->AddTask([sessao] {
                    g_tarefaPendente.store(false);
                    if (sessao == g_geracao.load()) {
                        AtualizarControleCombate();
                    }
                });
            }
        });
    }
}
