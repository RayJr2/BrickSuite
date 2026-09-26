cmake_minimum_required(VERSION 3.16)
project(BlenderOverrideProof LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Sql)
get_filename_component(ROOT "../.." ABSOLUTE)
set(RELEASE "${ROOT}/build/Desktop_Qt_6_10_3_MinGW_64_bit-Release")
# Reuse already-built, unchanged production objects. Never rebuild/patch them here.
file(GLOB_RECURSE OBJECTS "${RELEASE}/CMakeFiles/LocalPrintableOverrideServiceTest.dir/src/*.obj")
add_executable(BlenderOverrideProof validate.cpp ${OBJECTS})
target_include_directories(BlenderOverrideProof PRIVATE "${ROOT}")
target_link_libraries(BlenderOverrideProof PRIVATE Qt6::Core Qt6::Gui Qt6::Sql
    "${RELEASE}/lib/libmcut.dll.a" "${RELEASE}/_deps/lib3mf-build/lib3mf.a")
