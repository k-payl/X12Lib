mkdir build
cd build
"C:\Program Files\CMake\bin\cmake.exe" -DVK_ENABLE=OFF -G "Visual Studio 16 2019" ../
cd ..
mklink x12lib.sln build\x12lib.sln
