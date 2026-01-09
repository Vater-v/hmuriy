@echo off
setlocal

:: --- НАСТРОЙКИ ---
:: Укажите здесь ваш реальный путь к NDK
set "NDK_PATH=C:\android-ndk-r29"

:: Папки
set "BUILD_DIR=build"
set "OUT_DIR=valera"

:: --- ПРОВЕРКИ ---
if not exist "%NDK_PATH%" (
    echo [ERROR] NDK path not found at: %NDK_PATH%
    echo Please edit the .bat file and set the correct NDK_PATH.
    pause
    exit /b 1
)

:: Проверка наличия Ninja (обычно нужен для сборки NDK через CMake)
where ninja >nul 2>nul
if %errorlevel% neq 0 (
    echo [WARNING] 'ninja.exe' not found in PATH. Build might fail if CMake cannot find it.
    echo It is usually located in specific Android SDK folders or needs to be installed.
)

:: 1. Очистка предыдущих сборок
if exist "%BUILD_DIR%" rd /s /q "%BUILD_DIR%"
if exist "%OUT_DIR%" rd /s /q "%OUT_DIR%"

echo [INFO] Configuring for Android (ARM64)...

:: 2. Конфигурация CMake
:: Используем Ninja генератор. Флаг -DCMAKE_SYSTEM_NAME=Android обязателен или через toolchain
cmake -B "%BUILD_DIR%" -G "Ninja" ^
    -DCMAKE_TOOLCHAIN_FILE="%NDK_PATH%\build\cmake\android.toolchain.cmake" ^
    -DANDROID_ABI="arm64-v8a" ^
    -DANDROID_PLATFORM=android-21 ^
    -DCMAKE_BUILD_TYPE=Release

if %errorlevel% neq 0 (
    echo [ERROR] CMake configuration failed.
    pause
    exit /b 1
)

echo [INFO] Building...

:: 3. Сборка
cmake --build "%BUILD_DIR%"

if %errorlevel% neq 0 (
    echo [ERROR] Build failed.
    pause
    exit /b 1
)

:: 4. Создание папки valera и копирование результата
if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

echo [INFO] Copying hmuriy.so to %OUT_DIR%...

:: Рекурсивно ищем файл hmuriy.so в папке build
set FOUND=0
for /r "%BUILD_DIR%" %%f in (hmuriy.so) do (
    copy "%%f" "%OUT_DIR%\" >nul
    set FOUND=1
)

if %FOUND% equ 0 (
    echo [ERROR] hmuriy.so was not found in the build directory!
    pause
    exit /b 1
)

:: 5. Удаление мусора (папки build)
echo [INFO] Cleaning up build artifacts...
rd /s /q "%BUILD_DIR%"

echo.
echo [SUCCESS] Done! folder '%OUT_DIR%' contains 'hmuriy.so'.
pause