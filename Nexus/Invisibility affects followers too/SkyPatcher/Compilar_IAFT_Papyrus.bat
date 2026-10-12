
@echo off
setlocal EnableExtensions DisableDelayedExpansion

rem Deixe este BAT na pasta SkyPatcher (raiz do pacote).
rem Edite: Scripts\Source\IAFT_InvisibilitySync.psc
rem Saidas: Scripts\IAFT_InvisibilitySync.pex e copias para Data\Scripts e Data\Source\Scripts do Skyrim

set "ROOT=%~dp0"
set "SCRIPT=IAFT_InvisibilitySync"
set "PSC_DIR=%ROOT%Scripts\Source"
set "PEX_DIR=%ROOT%Scripts"
set "BUILD_DIR=%ROOT%_PapyrusBuild"

set "GAME_PEX_DIR=C:\Program Files (x86)\Steam\steamapps\common\Skyrim Special Edition\Data\Scripts"
set "GAME_PSC_DIR=C:\Program Files (x86)\Steam\steamapps\common\Skyrim Special Edition\Data\Source\Scripts"

set "CONFIG=%ROOT%PapyrusGamePath.txt"
set "GAME_DIR="

if not exist "%PSC_DIR%\%SCRIPT%.psc" (
    echo ERRO: arquivo-fonte nao encontrado:
    echo "%PSC_DIR%\%SCRIPT%.psc"
    goto :failed
)

rem Reutiliza o caminho do Skyrim salvo pelo BAT anterior.
if exist "%CONFIG%" set /p "GAME_DIR="<"%CONFIG%"

if not defined GAME_DIR if defined SKYRIM_SE_PATH set "GAME_DIR=%SKYRIM_SE_PATH%"

if not defined GAME_DIR if exist "C:\Program Files (x86)\Steam\steamapps\common\Skyrim Special Edition\Papyrus Compiler\PapyrusCompiler.exe" set "GAME_DIR=C:\Program Files (x86)\Steam\steamapps\common\Skyrim Special Edition"

if not defined GAME_DIR (
    echo Digite o caminho da pasta do Skyrim Special Edition:
    echo Exemplo: D:\SteamLibrary\steamapps\common\Skyrim Special Edition
    set /p "GAME_DIR=Caminho: "
)

set "GAME_DIR=%GAME_DIR:"=%"
if not defined GAME_DIR goto :failed

set "COMPILER=%GAME_DIR%\Papyrus Compiler\PapyrusCompiler.exe"

if not exist "%COMPILER%" (
    echo ERRO: compilador nao encontrado:
    echo "%COMPILER%"
    echo Verifique o caminho em "%CONFIG%".
    goto :failed
)

rem Procura as fontes padrao do Skyrim/Creation Kit.
set "IMPORTS=%PSC_DIR%"
set "FLAGS_FOUND="

if exist "%GAME_DIR%\Data\Source\Scripts" set "IMPORTS=%IMPORTS%;%GAME_DIR%\Data\Source\Scripts"
if exist "%GAME_DIR%\Data\Scripts\Source" set "IMPORTS=%IMPORTS%;%GAME_DIR%\Data\Scripts\Source"

if exist "%GAME_DIR%\Data\Source\Scripts\TESV_Papyrus_Flags.flg" set "FLAGS_FOUND=1"
if exist "%GAME_DIR%\Data\Scripts\Source\TESV_Papyrus_Flags.flg" set "FLAGS_FOUND=1"

if not defined FLAGS_FOUND (
    echo ERRO: TESV_Papyrus_Flags.flg nao encontrado nas fontes do Skyrim.
    echo Instale/extraia as fontes Papyrus do Creation Kit.
    goto :failed
)

rem Grava o caminho apos verificar o compilador e as fontes.
>"%CONFIG%" echo(%GAME_DIR%

if not exist "%PEX_DIR%" mkdir "%PEX_DIR%" || goto :failed
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%" || goto :failed

rem Compilacao isolada: preserva o PEX antigo se houver erro.
if exist "%BUILD_DIR%\%SCRIPT%.pex" del /q "%BUILD_DIR%\%SCRIPT%.pex"

pushd "%PSC_DIR%" || goto :failed

echo.
echo Compilando "%SCRIPT%.psc"...

"%COMPILER%" "%SCRIPT%.psc" -i="%IMPORTS%" -o="%BUILD_DIR%" -f="TESV_Papyrus_Flags.flg"

set "COMPILER_EXIT=%ERRORLEVEL%"
popd

if not "%COMPILER_EXIT%"=="0" (
    echo ERRO: compilacao falhou. O PEX anterior foi preservado.
    goto :failed
)

if not exist "%BUILD_DIR%\%SCRIPT%.pex" (
    echo ERRO: compilador nao gerou o PEX esperado.
    goto :failed
)

rem Atualiza o PEX do pacote.
copy /y "%BUILD_DIR%\%SCRIPT%.pex" "%PEX_DIR%\%SCRIPT%.pex" >nul || (
    echo ERRO: nao foi possivel atualizar o PEX do pacote.
    goto :failed
)

rem Copia PEX e PSC para a instalacao do Skyrim.
if not exist "C:\Program Files (x86)\Steam\steamapps\common\Skyrim Special Edition\Data" (
    echo ERRO: pasta Data do Skyrim nao encontrada.
    echo O PEX do pacote foi atualizado normalmente.
    goto :failed
)

rem Copia o PEX para Data\Scripts.
if not exist "%GAME_PEX_DIR%" mkdir "%GAME_PEX_DIR%" || goto :game_copy_failed

copy /y "%BUILD_DIR%\%SCRIPT%.pex" "%GAME_PEX_DIR%\%SCRIPT%.pex" >nul || goto :game_copy_failed

rem Copia o PSC para Data\Source\Scripts.
if not exist "%GAME_PSC_DIR%" mkdir "%GAME_PSC_DIR%" || goto :game_source_copy_failed

copy /y "%PSC_DIR%\%SCRIPT%.psc" "%GAME_PSC_DIR%\%SCRIPT%.psc" >nul || goto :game_source_copy_failed

echo.
echo ==========================================
echo  COMPILACAO E COPIAS CONCLUIDAS!
echo ==========================================
echo.
echo Fonte PSC: "%PSC_DIR%\%SCRIPT%.psc"
echo PEX do pacote: "%PEX_DIR%\%SCRIPT%.pex"
echo PEX do jogo: "%GAME_PEX_DIR%\%SCRIPT%.pex"
echo PSC do jogo: "%GAME_PSC_DIR%\%SCRIPT%.psc"
echo.

pause
exit /b 0

:game_copy_failed
echo.
echo ERRO: nao foi possivel copiar o PEX para o Skyrim.
echo Destino: "%GAME_PEX_DIR%\%SCRIPT%.pex"
echo O PEX do pacote ja foi atualizado.
echo Se necessario, execute o BAT como administrador.
goto :failed

:game_source_copy_failed
echo.
echo ERRO: nao foi possivel copiar o PSC para o Skyrim.
echo Destino: "%GAME_PSC_DIR%\%SCRIPT%.psc"
echo O PEX do pacote e o PEX do jogo ja foram atualizados.
echo Se necessario, execute o BAT como administrador.
goto :failed

:failed
echo.
echo Operacao nao concluida.
pause
exit /b 1
