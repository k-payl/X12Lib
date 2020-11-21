cmake_minimum_required (VERSION 3.4.3)

# cd build
# "C:\Program Files\CMake\bin\cmake.exe" -G "Visual Studio 16 2019" ../

# Options:
# VK_ENABLE
# TESTS_ENABLE

project (x12lib)

option(TESTS_ENABLE "" ON)

# YAML
option(YAML_CPP_BUILD_TOOLS "" OFF)
option(YAML_CPP_BUILD_TESTS "" OFF)
add_subdirectory(3rdparty/yaml-cpp)

option(VK_ENABLE "Enable vulkan renderer" OFF)

if (VK_ENABLE)
	find_package(Vulkan REQUIRED FATAL_ERROR)
	if (NOT Vulkan_FOUND)
		message(FATAL_ERROR "Vulkan not find!")
	else()
		message(STATUS "Vulkan lib path: " ${Vulkan_LIBRARY})
	endif()
endif()

set(CMAKE_SYSTEM_VERSION "10.0.19041.0" CACHE STRING INTERNAL FORCE)
set(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION "10.0.19041.0" CACHE STRING INTERNAL FORCE)


add_definitions ( -D_UNICODE )
add_definitions ( -DUNICODE )
remove_definitions ( -D_MBCS )
remove_definitions ( -DMBCS )

file(GLOB SOURCES_SYSTEM src/engine/system/*.cpp src/math/*.cpp)
file(GLOB SOURCES_VIDEO src/engine/x12/*.cpp src/engine/x12/d3d12/*.cpp)
file(GLOB SOURCES_3RDPARTY     
	3rdparty/*.cpp
    3rdparty/pugixml/*.cpp
    3rdparty/DirectXTK/Src/*.cpp
    3rdparty/DirectXTex/*.cpp)

file(GLOB HEADERS_VIDEO src/engine/x12/*.h src/engine/x12/d3d12/*.h include/engine/x12/d3d12/*.h include/engine/x12/*.h)
file(GLOB HEADERS_SYSTEM src/engine/system/*.h src/math/*.h include/engine/system/*.h include/math/*.h)
file(GLOB HEADERS_3RDPARTY
	3rdparty/*.h
    3rdparty/pugixml/*.h
    3rdparty/DirectXTK/Inc/*.h
    3rdparty/DirectXTex/*.h)
	
# Vulkan
if (VK_ENABLE)
	add_definitions(-DVK_ENABLE=1)
	file(GLOB SOURCES_VK src/engine/x12/vk/*.cpp)
	file(GLOB HEADERS_VK src/engine/x12/vk/*.h include/engine/x12/vk/*.h)
	set(SOURCES_VIDEO ${SOURCES_VIDEO} ${SOURCES_VK})
	set(HEADERS_VIDEO ${HEADERS_VIDEO} ${HEADERS_VK})
	set(HEADERS ${HEADERS} ${Vulkan_INCLUDE_DIR}/vulkan/)
endif()

set(SOURCES ${SOURCES} ${SOURCES_SYSTEM} ${SOURCES_VIDEO} ${SOURCES_3RDPARTY})
set(HEADERS ${HEADERS} "resources/shaders/")
set(HEADERS ${HEADERS} ${HEADERS_SYSTEM} ${HEADERS_VIDEO} ${HEADERS_3RDPARTY})
set(VIDEO_FILES ${SOURCES_VIDEO} ${HEADERS_VIDEO})
source_group("system" FILES ${SOURCES_SYSTEM} ${HEADERS_SYSTEM})
source_group("3rdparty" FILES ${SOURCES_3RDPARTY} ${HEADERS_3RDPARTY})
source_group("x12" FILES ${VIDEO_FILES})

# TODO: fix C: drive
set(DXC_PATH "C:\\Program Files (x86)\\Windows Kits\\10\\bin\\${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}\\x64\\dxcompiler.dll")
set(DXIL_PATH "C:\\Program Files (x86)\\Windows Kits\\10\\bin\\${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}\\x64\\dxil.dll")
message("DXC_PATH = ${DXC_PATH}")
message("DXIL_PATH = ${DXIL_PATH}")
#message("HEADERS_SYSTEM = ${HEADERS_SYSTEM}")
		
include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("/std:c++latest" _cpp_latest_flag_supported)
if (_cpp_latest_flag_supported)
    add_compile_options("/std:c++latest")
else()
	add_compile_options("/std:c++17")
endif()

# engine
add_library(engine SHARED ${SOURCES} ${HEADERS})

target_include_directories(engine PRIVATE
	include/engine/system
	include/engine/x12
	include/engine/x12/d3d12
	include/d3dx12
	include/math
	resources/shaders
	src/engine/3rdparty/yaml-cpp/include
	src/engine/x12 
	src/engine/x12/d3d12
	src/engine/system
	src/engine/math
	3rdparty/d3dx12
    3rdparty/pugixml
    3rdparty/DirectXTK/Inc
    3rdparty/DirectXTex)

set(ENGINE_LIBS dxguid dxgi d3d12 d3dcompiler)
if (VK_ENABLE)
	target_include_directories(engine PRIVATE 
		include/engine/x12/vk
		3rdparty/VulkanMemoryAllocator
		${Vulkan_INCLUDE_DIR}/vulkan)
	set(ENGINE_LIBS ${ENGINE_LIBS} ${Vulkan_LIBRARY})
endif()

target_precompile_headers(engine PRIVATE src/engine/system/pch.h)
target_link_libraries(engine ${ENGINE_LIBS})
target_link_libraries(engine yaml-cpp)
target_link_libraries(engine dxcompiler)
target_compile_definitions(engine PUBLIC _DLL_EXPORTS)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")

# Examples

# engine main
add_executable(engine_main src/examples/engine_main.cpp)
target_include_directories(engine_main PRIVATE
	include/engine/system
	include/engine/x12
	include/engine/x12/d3d12
	include/math
	resources/shaders
	3rdparty/d3dx12)
target_link_libraries(engine_main engine)
target_link_libraries(engine_main dxcompiler)
target_compile_features(engine_main PRIVATE cxx_std_17)
set_property(TARGET engine_main PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/build")
# copy dxcompiler.dll for raytracing
add_custom_command(TARGET engine_main POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
	"${DXC_PATH}"
	$<TARGET_FILE_DIR:engine_main>)

# copy dxil.dll for raytracing
add_custom_command(TARGET engine_main POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
	"${DXIL_PATH}"
	$<TARGET_FILE_DIR:engine_main>)


# constbuffer example
add_executable(constantbuffers src/examples/constantbuffers.cpp)
target_include_directories(constantbuffers PRIVATE
	include/engine/system
	include/engine/x12
	include/engine/x12/d3d12
	include/math
	resources/shaders
	3rdparty/d3dx12)
target_include_directories(constantbuffers PRIVATE src/examples)
target_link_libraries(constantbuffers engine)
set_property(TARGET constantbuffers PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/build")

# gltf mesh exporter
add_executable(gltfmeshexporter src/examples/gltfmeshexporter.cpp)
target_include_directories(gltfmeshexporter PRIVATE
	include/engine/system
	include/math
	include/engine/x12
	include/engine/x12/d3d12
	include/d3dx12
	3rdparty/tinygltf
	3rdparty/d3dx12
	src/engine/3rdparty/yaml-cpp/include
	resources/shaders)
target_link_libraries(gltfmeshexporter yaml-cpp)
target_link_libraries(gltfmeshexporter engine)
set_property(TARGET gltfmeshexporter PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/build")

# test
if (TESTS_ENABLE)
	set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
	add_subdirectory(3rdparty/googletest)

	add_executable(tests src/tests/tests_main.cpp)
	target_include_directories(tests PRIVATE
		include/engine/system
		include/engine/x12
		include/engine/x12/d3d12
		include/math
		resources/shaders
		3rdparty/d3dx12
		3rdparty/googletest/googletest/include
		3rdparty/stb)
	if (VK_ENABLE)
		target_include_directories(tests PRIVATE src/engine/x12/vk)
	endif()
	target_link_libraries(tests engine gtest_main)
	set_target_properties(tests PROPERTIES LINK_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:CONSOLE")
	set_property(TARGET tests PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/build")
endif()
