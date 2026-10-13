@echo off
setlocal enabledelayedexpansion
title Benchmark Criptografico - One-Click Launcher C Nativo (Windows x64)

echo ======================================================================
echo    SUITE DE BENCHMARKING CRIPTOGRAFICO (RSA vs. ML-KEM / Kyber)
echo    Universidad del Valle de Guatemala - Tesis de Grado (2026)
echo    Arquitectura 100%% C Nativo (Sin dependencias de Python)
echo ======================================================================
echo.

:: 1. Ubicarse en el directorio del script y localizar la raiz del proyecto
cd /d "%~dp0"
if exist "benchmark_criptografia\scripts\c\orquestador.c" (
    cd benchmark_criptografia
)
if not exist "scripts\c\orquestador.c" (
    if exist "..\benchmark_criptografia\scripts\c\orquestador.c" (
        cd ..\benchmark_criptografia
    )
)
if not exist "scripts\c\orquestador.c" (
    echo ======================================================================
    echo  [ERROR] No se encontraron los archivos fuentes: scripts\c\orquestador.c
    echo ======================================================================
    echo  Causa: Se ejecuto este archivo .bat de forma aislada sin la carpeta
    echo         completa del repositorio: scripts\, entorno\, bin\, etc.
    echo  Solucion: Extraiga o copie la carpeta completa del repositorio antes de ejecutar.
    echo ======================================================================
    pause
    exit /b 1
)

:: 2. Verificar existencia de binarios nativos compilados en C
echo [1/5] Verificando binarios compilados en C nativo...
set "NEED_BUILD=0"
if not exist "bin\orquestador.exe" set "NEED_BUILD=1"
if not exist "bin\inventario.exe" set "NEED_BUILD=1"
if not exist "bin\rsa_benchmark.exe" set "NEED_BUILD=1"
if not exist "bin\kyber_benchmark.exe" set "NEED_BUILD=1"

if "%NEED_BUILD%"=="1" (
    echo     Compilando binarios en C con CMake y MSVC...
    if exist "C:\liboqs\install\bin\oqs.dll" (
        copy /y "C:\liboqs\install\bin\oqs.dll" "bin\oqs.dll" >nul 2>&1
    )
    if exist "C:\Program Files\OpenSSL-Win64\bin\libcrypto-4-x64.dll" (
        copy /y "C:\Program Files\OpenSSL-Win64\bin\libcrypto-4-x64.dll" "bin\" >nul 2>&1
        copy /y "C:\Program Files\OpenSSL-Win64\bin\libssl-4-x64.dll" "bin\" >nul 2>&1
    )
    cmake -S . -B build_win -DOPENSSL_ROOT_DIR="C:\Program Files\OpenSSL-Win64" -DLIBOQS_DIR="C:\liboqs\install" >nul 2>&1
    cmake --build build_win --config Release >nul 2>&1
)

if not exist "bin\orquestador.exe" (
    echo ======================================================================
    echo  [ERROR] No se localizo bin\orquestador.exe y no se pudo autocompilar.
    echo  Verifique que Visual Studio con herramientas C/C++ y CMake esten instalados.
    echo ======================================================================
    pause
    exit /b 1
)
echo     Binarios nativos en C verificados exitosamente [OK].

:: 3. Cerrar procesos no esenciales para estabilizar CPU y RAM
echo.
echo ======================================================================
echo [2/5] ESTABILIZACION DEL ENTORNO DE PRUEBA (Windows)
echo ======================================================================
echo Se cerraran aplicaciones no esenciales en segundo plano que puedan
echo generar interrupciones o picos de CPU durante el experimento:
echo  * Navegadores (Chrome, Edge, Firefox, Brave, Opera)
echo  * Mensajeria/Juegos (Discord, Slack, Teams, Spotify, Steam, WhatsApp)
echo  * Sincronizacion (OneDrive, Dropbox, Google Drive)
echo.
echo Si tiene documentos o trabajo abierto en esas aplicaciones, presione
echo Ctrl+C en los proximos 5 segundos para cancelar.
echo.
timeout /t 5 /nobreak >nul 2>&1 || ping 127.0.0.1 -n 6 >nul

echo Cerrando procesos en segundo plano...
taskkill /f /im chrome.exe >nul 2>&1
taskkill /f /im msedge.exe >nul 2>&1
taskkill /f /im firefox.exe >nul 2>&1
taskkill /f /im brave.exe >nul 2>&1
taskkill /f /im opera.exe >nul 2>&1
taskkill /f /im Discord.exe >nul 2>&1
taskkill /f /im Slack.exe >nul 2>&1
taskkill /f /im Teams.exe >nul 2>&1
taskkill /f /im msteams.exe >nul 2>&1
taskkill /f /im Spotify.exe >nul 2>&1
taskkill /f /im WhatsApp.exe >nul 2>&1
taskkill /f /im Telegram.exe >nul 2>&1
taskkill /f /im Steam.exe >nul 2>&1
taskkill /f /im EpicGamesLauncher.exe >nul 2>&1
taskkill /f /im OneDrive.exe >nul 2>&1
taskkill /f /im Dropbox.exe >nul 2>&1
taskkill /f /im GoogleDriveFS.exe >nul 2>&1
taskkill /f /im Zoom.exe >nul 2>&1
echo     Procesos en segundo plano finalizados.

:: Activar plan de Alto Rendimiento
echo Optimizando plan de energia a Alto Rendimiento...
powercfg -setactive 8c5e7fda-e8bf-4a96-9a15-5eec08ebd950 >nul 2>&1
if %errorlevel% neq 0 (
    powercfg -setactive SCHEME_MIN >nul 2>&1
)
echo     Plan de energia configurado.

:: 4. Generar fichas de inventario de hardware y versiones con C nativo
echo.
echo [3/5] Registrando inventario del dispositivo en C nativo...
bin\inventario.exe
echo.

:: 5. Menu de seleccion de modo de ejecucion
echo ======================================================================
echo [4/5] CONFIGURACION DE LA SERIE DE EJECUCIONES (SUITE C PURA)
echo ======================================================================
echo Seleccione el modo que desea ejecutar:
echo   [1] Prueba Piloto - 200 repeticiones por nivel (Recomendado)
echo   [2] Serie Formal Completa - ejecucion_01 y ejecucion_02 (5,000 rep.)
echo   [3] Solo Ejecucion Formal 01 (5,000 repeticiones)
echo   [4] Solo Ejecucion Formal 02 de Replicacion (5,000 repeticiones)
echo.
set /p MODO_ELEGIDO="Ingrese opcion [1-4] (defecto 1): "
if "%MODO_ELEGIDO%"=="" set MODO_ELEGIDO=1

set /p DISPOSITIVO_ID="Identificador de este equipo [defecto pc_windows]: "
if "%DISPOSITIVO_ID%"=="" set DISPOSITIVO_ID=pc_windows

echo.
echo Reposo termico de 5 segundos previo al inicio...
timeout /t 5 /nobreak >nul 2>&1 || ping 127.0.0.1 -n 6 >nul

if "%MODO_ELEGIDO%"=="1" (
    echo Iniciando Prueba Piloto: 200 ejecuciones por algoritmo...
    start /high /wait bin\orquestador.exe --mode pilot --device !DISPOSITIVO_ID! --ejecucion piloto
)

if "%MODO_ELEGIDO%"=="2" (
    echo Iniciando Ejecucion Formal 01: 5,000 ejecuciones por algoritmo...
    start /high /wait bin\orquestador.exe --mode full --device !DISPOSITIVO_ID! --ejecucion ejecucion_01
    
    echo.
    echo ==================================================================
    echo Pausa de enfriamiento termico de 60 segundos antes de replicar...
    echo ==================================================================
    timeout /t 60 /nobreak >nul 2>&1 || ping 127.0.0.1 -n 61 >nul
    
    echo Iniciando Ejecucion Formal 02 de Replicacion: 5,000 ejecuciones...
    start /high /wait bin\orquestador.exe --mode full --device !DISPOSITIVO_ID! --ejecucion ejecucion_02
)

if "%MODO_ELEGIDO%"=="3" (
    echo Iniciando Ejecucion Formal 01: 5,000 ejecuciones por algoritmo...
    start /high /wait bin\orquestador.exe --mode full --device !DISPOSITIVO_ID! --ejecucion ejecucion_01
)

if "%MODO_ELEGIDO%"=="4" (
    echo Iniciando Ejecucion Formal 02: 5,000 ejecuciones por algoritmo...
    start /high /wait bin\orquestador.exe --mode full --device !DISPOSITIVO_ID! --ejecucion ejecucion_02
)

echo.
echo ======================================================================
echo [5/5] PROCESO COMPLETADO EXITOSAMENTE
echo  Resultados almacenados en: %CD%\resultados
echo.
echo  Para consolidar graficos y tablas de tesis, ejecute posteriormente:
echo  python scripts\analisis\graficar_tesis.py
echo ======================================================================
echo.
pause
