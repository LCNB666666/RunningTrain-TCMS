@echo off
rem compile TCMS screen (MinGW gcc/g++ + GDI+)
rem 需要: MinGW-w64 (gcc/g++) 在 PATH 中
g++ -O2 -c gfx_gdiplus.cpp -o gfx_gdiplus.o
if errorlevel 1 goto err
gcc -O2 -c main.c -o main.o
if errorlevel 1 goto err
gcc -O2 -c tcms_screen.c -o tcms_screen.o
if errorlevel 1 goto err
g++ -mwindows -o tcms_screen.exe main.o tcms_screen.o gfx_gdiplus.o -lgdiplus
if errorlevel 1 goto err
echo OK: tcms_screen.exe
goto end
:err
echo COMPILE FAILED
:end
