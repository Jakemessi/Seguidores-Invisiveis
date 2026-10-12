@echo off
setlocal EnableExtensions

rem Coloque este build.bat na raiz de Seguidores-Invisiveis.
rem Usa a configuracao CMake existente em build\release.
cd /d "%~dp0"

set "CMAKE="
where cmake >nul 2>&1
if not errorlevel 1 set "CMAKE=cmake"
if not defined CMAKE if exist "C:\Programas\CMake\bin\cmake.exe" set "CMAKE=C:\Programas\CMake\bin\cmake.exe"
if not defined CMAKE if exist "C:\Program Files\CMake\bin\cmake.exe" set "CMAKE=C:\Program Files\CMake\bin\cmake.exe"

if not defined CMAKE (
    echo ERRO: CMake nao encontrado.
    echo Instale o CMake ou adicione-o ao PATH.
    goto :erro
)

if not exist "build\release\CMakeCache.txt" (
    echo ERRO: configuracao CMake nao encontrada em build\release.
    echo Configure o projeto antes de executar este BAT.
    goto :erro
)

rem Prepara o compilador MSVC quando executado em um CMD comum.
where cl >nul 2>&1
if errorlevel 1 (
    if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" (
        call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
    )
)

echo.
echo Compilando InvisibilityAffectsFollowersToo - Release...
echo.
"%CMAKE%" --build "build\release" --config Release --parallel 4
if errorlevel 1 goto :erro

echo.
echo ==========================================
echo  BUILD CONCLUIDA COM SUCESSO!
echo ==========================================
echo.
pause
exit /b 0

:erro
echo.
echo Build nao concluida.
pause
exit /b 1
