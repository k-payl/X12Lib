mkdir build
cd build
"C:\Program Files\CMake\bin\cmake.exe" -DVK_ENABLE=ON -DRAYTRACING_SAMPLE=ON -G "Visual Studio 16 2019" ../
cd ..
mklink x12lib.sln build\x12lib.sln
