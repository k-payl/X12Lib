cd build
"C:\Program Files\CMake\bin\cmake.exe" -G "Visual Studio 16 2019" ../
rem -DVK_ENABLE=ON

cd ..
mklink x12lib.sln build\x12lib.sln
