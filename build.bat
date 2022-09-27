@echo off
setlocal enabledelayedexpansion

REM Find and setup vs build tools.
where /Q cl.exe || (
    for /f "tokens=*" %%i in ('"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath') do set VisualStudio=%%i
    if "!VisualStudio!" equ "" (
        echo ERROR: Visual Studio installation not found
        exit /b 1
    )
    call "!VisualStudio!\VC\Auxiliary\Build\vcvarsall.bat" x64 || exit /b
)

set compile_flags= -Od -MTd -nologo -fp:fast -fp:except- -Gm- -GR- -EHa- -Zo -Oi -Z7 /FC -wd4201 -wd4505 -wd4100 -wd4189 -wd4127 -wd4311
set link_flags= gdi32.lib user32.lib winmm.lib ole32.lib opengl32.lib dwrite.lib -opt:ref -incremental:no /Debug:full

if not exist build mkdir build
pushd build

start /b /wait "" "cl.exe"  %build_options% %compile_flags% ../src/dwrite_rasterize.cpp /link %link_flags% /out:rasterize.exe
