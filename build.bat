@echo off
setlocal enabledelayedexpansion

:: ==========================================
::               НАСТРОЙКИ
:: ==========================================

:: 1. Основные пути
set "ROOT_DIR=C:\valera"
set "PROJECT_SOURCE=%ROOT_DIR%\hmuriy"
set "UNPACKED_DIR=%ROOT_DIR%\unpacked_project"
set "NDK_PATH=C:\android-ndk-r29"

:: 2. Инструменты (лежат в C:\valera)
set "APKTOOL_JAR=%ROOT_DIR%\apktool.jar"
set "SIGNER_JAR=%ROOT_DIR%\uber-apk-signer.jar"

:: 3. Названия файлов
set "LIB_NAME=libhmuriy.so"
set "UNSIGNED_APK=%ROOT_DIR%\temp_unsigned.apk"
set "FINAL_APK_DIR=%ROOT_DIR%\release_out"

:: 4. Папка сборки CMake
set "BUILD_DIR=%PROJECT_SOURCE%\build"

:: ==========================================
::               ПРОВЕРКИ
:: ==========================================

if not exist "%NDK_PATH%" (
    echo [ERROR] NDK path not found at: %NDK_PATH%
    pause
    exit /b 1
)
if not exist "%APKTOOL_JAR%" (
    echo [ERROR] Apktool not found at: %APKTOOL_JAR%
    pause
    exit /b 1
)
if not exist "%SIGNER_JAR%" (
    echo [ERROR] Uber-signer not found at: %SIGNER_JAR%
    pause
    exit /b 1
)

:: Проверка наличия Ninja
where ninja >nul 2>nul
if %errorlevel% neq 0 (
    echo [WARNING] 'ninja.exe' not found in PATH. Build might fail if CMake cannot find it.
)

:: Очистка старой папки сборки C++
if exist "%BUILD_DIR%" rd /s /q "%BUILD_DIR%"

:: ==========================================
:: ШАГ 1: Сборка .so библиотеки (C++)
:: ==========================================
echo.
echo [STEP 1/5] Building native library (ARM64)...
echo ------------------------------------------

cmake -B "%BUILD_DIR%" -G "Ninja" ^
    -DCMAKE_TOOLCHAIN_FILE="%NDK_PATH%\build\cmake\android.toolchain.cmake" ^
    -DANDROID_ABI="arm64-v8a" ^
    -DANDROID_PLATFORM=android-21 ^
    -DCMAKE_BUILD_TYPE=Release

if %errorlevel% neq 0 goto :BuildError

cmake --build "%BUILD_DIR%"
if %errorlevel% neq 0 goto :BuildError

:: ==========================================
:: ШАГ 2: Копирование .so в распакованный проект
:: ==========================================
echo.
echo [STEP 2/5] Injecting library into unpacked project...
echo ------------------------------------------

set "TARGET_LIB_DIR=%UNPACKED_DIR%\lib\arm64-v8a"
if not exist "%TARGET_LIB_DIR%" mkdir "%TARGET_LIB_DIR%"

:: Ищем любой .so файл и копируем с правильным именем
set FOUND=0
for /r "%BUILD_DIR%" %%f in (*hmuriy.so) do (
    echo Copying "%%f" to "%TARGET_LIB_DIR%\%LIB_NAME%"
    copy /Y "%%f" "%TARGET_LIB_DIR%\%LIB_NAME%" >nul
    set FOUND=1
)

if %FOUND% equ 0 (
    echo [ERROR] .so library was not found in build artifacts!
    goto :Error
)

:: ==========================================
:: ШАГ 3: Сборка APK через Apktool
:: ==========================================
echo.
echo [STEP 3/5] Rebuilding APK with Apktool...
echo ------------------------------------------

:: Удаляем старый временный файл, если есть
if exist "%UNSIGNED_APK%" del "%UNSIGNED_APK%"

java -jar "%APKTOOL_JAR%" b "%UNPACKED_DIR%" -o "%UNSIGNED_APK%"
if %errorlevel% neq 0 (
    echo [ERROR] Apktool build failed.
    goto :Error
)

:: ==========================================
:: ШАГ 4: Подпись APK (Uber Apk Signer)
:: ==========================================
echo.
echo [STEP 4/5] Signing APK...
echo ------------------------------------------

:: Очищаем папку вывода перед подписью
if exist "%FINAL_APK_DIR%" rd /s /q "%FINAL_APK_DIR%"
mkdir "%FINAL_APK_DIR%"

:: ИСПРАВЛЕНИЕ: Убран флаг --overwrite, так как есть --out
java -jar "%SIGNER_JAR%" --apks "%UNSIGNED_APK%" --out "%FINAL_APK_DIR%"

if %errorlevel% neq 0 (
    echo [ERROR] Signing failed.
    goto :Error
)

:: Находим подписанный файл (имя может меняться, ищем первый apk в папке вывода)
for %%f in ("%FINAL_APK_DIR%\*.apk") do set "SIGNED_APK=%%f"

if not defined SIGNED_APK (
    echo [ERROR] Signed APK not found in output directory.
    goto :Error
)
echo [INFO] Signed APK ready: %SIGNED_APK%

:: ==========================================
:: ШАГ 5: Установка через ADB
:: ==========================================
echo.
echo [STEP 5/5] Installing via ADB...
echo ------------------------------------------

adb install -r "%SIGNED_APK%"
if %errorlevel% neq 0 (
    echo [ERROR] ADB Install failed. Check if device is connected and debugging is on.
    goto :Error
)

:: ==========================================
:: ОЧИСТКА МУСОРА
:: ==========================================
echo.
echo [CLEANUP] Removing temporary files...

:: Удаляем папку сборки C++
if exist "%BUILD_DIR%" rd /s /q "%BUILD_DIR%"
:: Удаляем неподписанный APK
if exist "%UNSIGNED_APK%" del "%UNSIGNED_APK%"

echo.
echo ==========================================
echo [SUCCESS] Pipeline completed successfully!
echo ==========================================
pause
exit /b 0

:BuildError
echo [ERROR] C++ Build failed.
pause
exit /b 1

:Error
echo.
echo [FAIL] Pipeline stopped due to an error.
pause
exit /b 1