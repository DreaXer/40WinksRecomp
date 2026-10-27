@echo off
rem Configure (first run) and build 40WinksRecompiled.exe into build-vs\ with Visual Studio's clang-cl, cmake and ninja.
rem Regenerate RecompiledFuncs\ (N64Recomp.exe 40winks.toml) and rsp\ (RSPRecomp.exe aspMain.toml) before, if the ELF changed.
setlocal
set VS=C:\Program Files\Microsoft Visual Studio\18\Community
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
rem Visual Studio's cmake/ninja must come first: other cmake builds on PATH break the rc.exe step (RC1107).
set PATH=%VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%
set VCPKG_ROOT=%VS%\VC\vcpkg
cd /d "%~dp0"
if not exist build-vs\build.ninja (
    cmake -S . -B build-vs -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl || exit /b 1
)
cmake --build build-vs --target 40WinksRecompiled -j8
