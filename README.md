# Seguidores Invisíveis

Meu primeiro plugin SKSE para Skyrim.

Versão atual: **6.0.0**

## Sobre

Seguidores Invisíveis faz com que os seguidores do jogador sejam muito mais difíceis de detectar enquanto o jogador estiver:

- Agachado;
- Invisível.

O objetivo é evitar que os seguidores prejudiquem situações de furtividade enquanto acompanham o jogador.

## O que este plugin faz?

Enquanto o jogador estiver agachado ou invisível, o plugin aplica temporariamente aos seguidores:

- Um grande bônus de Sneak;
- Um bônus de invisibilidade utilizado pelo sistema do plugin.

Enquanto o jogador estiver invisível, os seguidores também recebem temporariamente a flag:

`Doesn't Affect Stealth Meter`

Isso impede que os seguidores interfiram no medidor de detecção do jogador durante a invisibilidade.

Quando o efeito deixa de ser necessário, os valores temporários são removidos e o estado original das flags dos seguidores é restaurado.

O plugin considera como seguidores os atores marcados pelo jogo como integrantes da equipe do jogador (`Player Teammate`).

## Compatibilidade

O plugin é compilado utilizando a versão mantida por **alandtse** da CommonLibSSE-NG em configuração multi-runtime.

A mesma DLL é compilada com suporte a:

- Skyrim Special Edition (SE);
- Skyrim Anniversary Edition (AE);
- Skyrim VR.

A build atual utiliza o branch `ng` da CommonLibSSE-NG.

### Runtime testado

A última versão do Skyrim em que o funcionamento do plugin foi efetivamente validado em runtime foi:

**Skyrim AE 1.6.1170**

A migração para a CommonLibSSE-NG mantida por alandtse também prepara o projeto para versões mais recentes do Skyrim, incluindo a linha 1.7.x.

Entretanto, o funcionamento deste plugin no Skyrim 1.7.x **ainda não foi validado em runtime neste projeto**.

Portanto, a compatibilidade com essas versões não deve ser considerada oficialmente testada até que os testes no jogo sejam realizados.

## Requisitos

Para utilizar a implementação SKSE são necessários:

- SKSE compatível com a versão do Skyrim utilizada;
- Address Library compatível com a versão do Skyrim utilizada.

O instalador também contém arquivos opcionais relacionados à implementação via SkyPatcher.

## CommonLibSSE-NG

Este projeto utiliza:

https://github.com/alandtse/CommonLibSSE-NG

A CommonLibSSE-NG é integrada diretamente ao projeto como um submodule Git localizado em:

`extern/CommonLibSSE-NG`

O projeto acompanha o branch:

`ng`

O commit da CommonLib utilizado por cada versão do projeto fica fixado pelo próprio submodule Git, permitindo builds reproduzíveis mesmo que o branch `ng` continue recebendo atualizações.

A CommonLib é compilada junto com o plugin em modo multi-runtime, com:

- Skyrim SE habilitado;
- Skyrim AE habilitado;
- Skyrim VR habilitado;
- Patch Safety Checks habilitado;
- Xbyak desabilitado;
- Testes internos da CommonLib desabilitados.

## Dependências de build

As dependências externas utilizadas pelo projeto incluem:

- CommonLibSSE-NG;
- DirectXTK;
- rapidcsv;
- spdlog;
- OpenVR, utilizado internamente pela CommonLibSSE-NG para suporte ao Skyrim VR.

As dependências auxiliares são gerenciadas através do vcpkg.

A própria CommonLibSSE-NG é mantida como submodule Git e não é mais obtida através do antigo port `commonlibsse-ng` do registry Color-Glass.

## Compilação

O projeto utiliza:

- CMake;
- Ninja;
- MSVC / Visual Studio 2022 Build Tools;
- vcpkg;
- C++23.

A CommonLibSSE-NG é compilada diretamente junto com o projeto.

Exemplo de estrutura:

    Seguidores-Invisiveis/
    ├── extern/
    │   └── CommonLibSSE-NG/
    ├── src/
    ├── Nexus/
    ├── CMakeLists.txt
    ├── vcpkg.json
    └── vcpkg-configuration.json

Após clonar o repositório, os submodules devem ser inicializados com:

    git submodule update --init --recursive

Isso também inicializa as dependências internas necessárias da CommonLibSSE-NG, como o OpenVR.

## DLL

O plugin principal gerado é:

`InvisibilityAffectsFollowersToo.dll`

A DLL exporta as interfaces esperadas pelo SKSE:

- `SKSEPlugin_Load`
- `SKSEPlugin_Query`
- `SKSEPlugin_Version`

Os metadados do plugin são gerados através do sistema fornecido pela CommonLibSSE-NG.

## Observação sobre detecção de invisibilidade

A atualização dos efeitos dos seguidores ocorre a partir dos eventos utilizados pelo plugin.

Em determinadas situações, se o jogador ganhar ou perder invisibilidade permanecendo completamente parado e nenhum evento relevante for disparado naquele momento, a atualização pode ocorrer somente após uma nova ação ou animação.

Esse comportamento poderá ser substituído futuramente por uma detecção específica de alterações de efeitos mágicos ou por outro mecanismo de atualização.

## Autor

Huehuller