@echo off
chcp 65001 >nul
:: ============================================================
:: STM32 智能温湿度监测系统 - Keil MDK 编译脚本
:: 用法:
::   build.bat          → 编译工程
::   build.bat flash    → 编译 + 烧录 (需 ST-Link Utility)
::   build.bat clean    → 清理编译产物
:: ============================================================

setlocal enabledelayedexpansion

set "PROJECT_NAME=STM32_TempHumidity"
set "PROJECT_FILE=%PROJECT_NAME%.uvprojx"
set "OUTPUT_DIR=.\STM32_TempHumidity\"
set "HEX_FILE=%OUTPUT_DIR%%PROJECT_NAME%.hex"
set "UV4_EXE=C:\Keil_v5\UV4\UV4.exe"
set "STLINK_CLI=C:\Program Files (x86)\STMicroelectronics\STM32 ST-LINK Utility\ST-LINK Utility\ST-LINK_CLI.exe"

echo ============================================================
echo   STM32 智能温湿度监测系统 - Keil MDK 编译脚本
echo ============================================================
echo.

:: 检查 Keil 是否安装
if not exist "%UV4_EXE%" (
    echo [警告] 未找到 Keil uVision5: %UV4_EXE%
    echo 请修改 UV4_EXE 变量指向 UV4.exe 的实际路径
    echo 常见路径:
    echo   C:\Keil_v5\UV4\UV4.exe
    echo   D:\Keil_v5\UV4\UV4.exe
    goto :usage
)

if "%1"=="clean" goto :clean
if "%1"=="flash" goto :flash

:: ===== 编译 =====
:build
echo [1/2] 正在编译工程...
echo.
"%UV4_EXE%" -b "%PROJECT_FILE%" -j0 -o build_log.txt
if %errorlevel% neq 0 (
    echo.
    echo [错误] 编译失败! 请查看 build_log.txt
    type build_log.txt
    exit /b 1
)
echo 编译成功!
echo.

:: 检查 hex 文件
if exist "%HEX_FILE%" (
    for %%A in ("%HEX_FILE%") do echo [2/2] HEX文件: %%A (%%~zA bytes^)
) else (
    echo [警告] 未找到 HEX 文件, 请在 Keil 中勾选 "Create HEX File"
)
echo.
echo ============================================================
echo   编译完成! 可将 HEX 文件加载到 Proteus 中仿真
echo   路径: %HEX_FILE%
echo ============================================================
goto :end

:: ===== 编译 + 烧录 =====
:flash
call :build
if %errorlevel% neq 0 exit /b 1

echo.
echo [3/3] 正在烧录到 STM32...
if not exist "%STLINK_CLI%" (
    echo [警告] 未找到 ST-LINK CLI, 请手动烧录 HEX 文件
    goto :end
)
"%STLINK_CLI%" -c SWD -P "%HEX_FILE%" 0x08000000 -V
if %errorlevel% equ 0 (
    echo 烧录成功! 复位目标板查看运行效果.
) else (
    echo 烧录失败! 请检查 ST-Link 连接.
)
goto :end

:: ===== 清理 =====
:clean
echo 清理编译产物...
if exist "%OUTPUT_DIR%" rmdir /s /q "%OUTPUT_DIR%"
if exist "build_log.txt" del /q "build_log.txt"
if exist "*.bak" del /q "*.bak"
if exist "*.dep" del /q "*.dep"
echo 清理完成!
goto :end

:: ===== 帮助 =====
:usage
echo.
echo 用法:
echo   build.bat          → 编译工程
echo   build.bat flash    → 编译 + 烧录
echo   build.bat clean    → 清理编译产物
echo.

:end
endlocal
