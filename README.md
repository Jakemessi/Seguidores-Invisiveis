# Seguidores Invisíveis

Meu primeiro plugin SKSE para Skyrim.

Versão atual: **6.0.0**

---

- [Compatibilidade](#compatibilidade)
- [O que este plugin faz?](#o-que-este-plugin-faz)
- [CommonLibSSE-NG](#commonlibsse-ng)

## Compatibilidade

O plugin é compilado com o CommonLibSSE-NG em modo multi-runtime, com suporte de build para:

- Skyrim Special Edition (SE)
- Skyrim Anniversary Edition (AE)
- Skyrim VR

O último runtime em que o funcionamento do plugin foi efetivamente testado foi o Skyrim Anniversary Edition **1.6.1170**, utilizando as versões correspondentes do SKSE e da Address Library.

Versões mais recentes, incluindo a linha **1.7.x**, ainda não foram validadas em jogo e não devem ser consideradas oficialmente suportadas até que os testes sejam concluídos.

## O que este plugin faz?

Este mod faz com que, quando o jogador entra em stealth, seus seguidores também recebam ajustes para reduzir drasticamente a capacidade dos inimigos de detectá-los.

O estado de stealth considerado pelo plugin inclui:

- jogador agachado;
- jogador sob efeito de invisibilidade.

Durante a invisibilidade do jogador, os seguidores também recebem temporariamente o comportamento **Doesn't Affect Stealth Meter**, com o estado original restaurado quando a invisibilidade termina.

## CommonLibSSE-NG

Este plugin utiliza o [CommonLibSSE-NG](https://github.com/CharmedBaryon/CommonLibSSE-NG) como base para integração com SKSE e acesso às estruturas internas do Skyrim.

A biblioteca é integrada ao projeto através do **vcpkg**, e o plugin é compilado utilizando o modo multi-runtime para Skyrim SE, AE e VR.

O CommonLibSSE-NG fornece abstrações para trabalhar com diferentes runtimes do Skyrim e reduz a necessidade de acessar diretamente endereços e estruturas internas específicas de uma única versão do jogo.

Este projeto utiliza **SKSE** e **Address Library**.
