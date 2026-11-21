#pragma once

#include <functional>

namespace RE
{
    class Actor;
}

namespace IAFT
{
    // Recebe o mesmo filtro de invocacoes/reanimados ja utilizado pela DLL.
    // As alteracoes de AI sao feitas exclusivamente na thread principal do jogo.
    void IniciarControleCombate(std::function<bool(RE::Actor*)> a_eInvocacaoAliada);
    void AtualizarControleCombate();
    void PararControleCombate();
}
