mkdir ../build/ -Force

cd ../build/

cmake .. `
    -DCMAKE_C_COMPILER=clang `
    -DCMAKE_CXX_COMPILER=clang++ `
    -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake

cmake --build .

mkdir ../bin/ -Force

Copy-Item -Path "Debug\*" -Destination ../bin/ -Force

cd ../scripts/