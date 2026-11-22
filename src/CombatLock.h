#pragma once

#include <functional>

// Declara o tipo Actor sem precisar incluir o arquivo completo dele.
namespace RE
{
    class Actor;
}

// Funcoes publicas para iniciar, atualizar e encerrar o controle de combate.
namespace IAFT
{
    // Recebe o mesmo filtro de invocacoes/reanimados ja utilizado pela DLL.
    // As alteracoes de AI sao feitas exclusivamente na thread principal do jogo.
    void IniciarControleCombate(std::function<bool(RE::Actor*)> a_eInvocacaoAliada);
    // Reavalia se as invocacoes devem atacar conforme o confronto atual.
    void AtualizarControleCombate();
    // Para o monitor e restaura os estados alterados.
    void PararControleCombate();
    // Mantem os valores originais dos AVs no co-save SKSE, inclusive apos carregar.
    void RegistrarSerializacaoAV();
}
