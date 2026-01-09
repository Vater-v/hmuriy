@echo off
setlocal

:: --- НАСТРОЙКИ ---
set "NDK_PATH=C:\android-ndk-r29"
set "BUILD_DIR=build"
set "OUT_DIR=release"

:: Проверка наличия NDK
if not exist "%NDK_PATH%" (
    echo [ERROR] NDK path not found at: %NDK_PATH%
    pause
    exit /b 1
)

:: 1. Очистка предыдущих сборок (на всякий случай)
if exist "%BUILD_DIR%" rd /s /q "%BUILD_DIR%"
if exist "%OUT_DIR%" rd /s /q "%OUT_DIR%"

echo [INFO] Configuring for Android (ARM64)...

:: 2. Конфигурация CMake
:: Мы указываем Toolchain файл из NDK, чтобы CMake знал, что собираем под Android
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

:: 4. Создание папки release и копирование результата
if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

echo [INFO] Copying libraries to %OUT_DIR%...
:: Ищем .so файлы в папке build и копируем их.
:: CMake по умолчанию добавляет префикс "lib", поэтому файл будет libvalera_loader.so
for /r "%BUILD_DIR%" %%f in (*.so) do copy "%%f" "%OUT_DIR%\" >nul

:: 5. Удаление мусора (папки build)
echo [INFO] Cleaning up build artifacts...
rd /s /q "%BUILD_DIR%"

echo.
echo [SUCCESS] Done! File is in the '%OUT_DIR%' folder.
pause