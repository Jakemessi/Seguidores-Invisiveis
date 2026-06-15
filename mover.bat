
@echo off
setlocal

REM Fonte relativa à pasta do script: SKSE\Plugins
set "SRC=%~dp0SKSE\Plugins"
set "DST=C:\Program Files (x86)\Steam\steamapps\common\Skyrim Special Edition\Data\SKSE\Plugins"

REM Criar destino se necessário
if not exist "%DST%" (
	mkdir "%DST%"
)

REM Mover todos os arquivos da pasta de plugins
if exist "%SRC%\*" (
	echo Movendo arquivos de "%SRC%" para "%DST%"...
	move "%SRC%\*" "%DST%" >nul
	echo Movidos.
) else (
	echo Nao ha arquivos em "%SRC%".
)

REM Perguntar se deve executar o skse64_loader.exe
set /p "CHOICE=Deseja executar o skse64_loader.exe? (S/N): "
if /I "%CHOICE%"=="S" (
	echo Executando skse64_loader.exe...
    cd /d "C:\Program Files (x86)\Steam\steamapps\common\Skyrim Special Edition\"
	start "" "skse64_loader.exe"
) else (
	echo Nao sera executado.
)

endlocal
