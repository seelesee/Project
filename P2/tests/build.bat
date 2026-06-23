@echo off
REM ============================================
REM  STM32 温湿度监测系统 — 单元测试编译脚本
REM ============================================
REM 自动查找 GCC 并编译运行测试
REM
REM 如果没有 GCC，请安装 MinGW-w64:
REM   下载: https://winlibs.com
REM   解压到 C:\mingw64，然后把 C:\mingw64\bin 加入 PATH
REM ============================================

setlocal enabledelayedexpansion

echo ============================================
echo   DHT22 驱动单元测试 — Windows 编译脚本
echo ============================================
echo.

REM 查找 GCC 编译器
set GCC_PATH=
for %%p in (
    "C:\mingw64\bin\gcc.exe"
    "C:\msys64\mingw64\bin\gcc.exe"
    "C:\msys64\ucrt64\bin\gcc.exe"
) do (
    if exist %%p (
        set GCC_PATH=%%~p
        echo [OK] 找到 GCC: %%p
        goto :found_gcc
    )
)

REM 尝试 PATH 中的 gcc
where gcc >nul 2>&1
if %errorlevel% equ 0 (
    set GCC_PATH=gcc
    echo [OK] 在 PATH 中找到 GCC
    goto :found_gcc
)

echo [错误] 未找到 GCC 编译器!
echo.
echo 请安装 MinGW-w64:
echo   方法1 (推荐): 下载 https://github.com/brechtsanders/winlibs_mingw/releases
echo             解压到 C:\mingw64, 将 C:\mingw64\bin 加入系统 PATH
echo   方法2: 安装 MSYS2 后运行:
echo           pacman -S mingw-w64-x86_64-gcc
echo.
pause
exit /b 1

:found_gcc
echo.
echo 正在编译测试程序...
"%GCC_PATH%" -Wall -Wextra -std=c11 -O0 -g -I. -I..\Inc -I..\ -o test_runner.exe test_dht22.c -lm

if %errorlevel% neq 0 (
    echo [失败] 编译出错，请检查错误信息
    pause
    exit /b 1
)

echo [成功] 编译完成 → test_runner.exe
echo.
echo ============================================
echo   运行测试...
echo ============================================
echo.

test_runner.exe

echo.
echo ============================================
echo   测试运行完毕
echo ============================================
pause
