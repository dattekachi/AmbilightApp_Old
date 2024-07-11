@echo off

mkdir build
cd build
set CMAKE_PREFIX_PATH=C:\Qt\6.2.2\msvc2019_64\lib\cmake\
cmake -DCMAKE_BUILD_TYPE=Release ..
set /p proceed=Co build app luon hay khong? (y/n): 
if /i "%proceed%"=="y" (
    cmake --build . --target package --config Release
    echo Build app da xong
)

echo Nhan phim bat ki de thoat
pause >nul