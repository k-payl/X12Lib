cmake_minimum_required (VERSION 3.4.3)

# cd build
# "C:\Program Files\CMake\bin\cmake.exe" -G "Visual Studio 16 2019" ../

# Options:
# RAYTRACING_SAMPLE
# VK_ENABLE

project (x12lib)

# YAML
option(YAML_CPP_BUILD_TOOLS "" OFF)
option(YAML_CPP_BUILD_TESTS "" OFF)
add_subdirectory(src/engine/3rdparty/yaml-cpp)

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

file(GLOB SOURCES_SYSTEM src/engine/system/*.cpp src/engine/math/*.cpp)
file(GLOB SOURCES_VIDEO src/engine/video/*.cpp src/engine/video/d3d12/*.cpp)
file(GLOB SOURCES_3RDPARTY     
	src/engine/3rdparty/*.cpp
    src/engine/3rdparty/pugixml/*.cpp
    src/engine/3rdparty/DirectXTK/Src/*.cpp
    src/engine/3rdparty/DirectXTex/*.cpp)

file(GLOB HEADERS_VIDEO src/engine/video/*.h src/engine/video/d3d12/*.h)
file(GLOB HEADERS_SYSTEM src/engine/system/*.h src/engine/math/*.h)
file(GLOB HEADERS_3RDPARTY
	src/engine/3rdparty/*.h
    src/engine/3rdparty/pugixml/*.h
    src/engine/3rdparty/DirectXTK/Inc/*.h
    src/engine/3rdparty/DirectXTex/*.h)
	
include_directories(src/engine/3rdparty/yaml-cpp/include)

# Vulkan
if (VK_ENABLE)
	add_definitions(-DVK_ENABLE=1)
	file(GLOB SOURCES_VK src/engine/video/vk/*.cpp)
	file(GLOB HEADERS_VK src/engine/video/vk/*.h)
	set(SOURCES_VIDEO ${SOURCES_VIDEO} ${SOURCES_VK})
	set(HEADERS_VIDEO ${HEADERS_VIDEO} ${HEADERS_VK})
	include_directories(src/engine/video/vk)
	include_directories(src/engine/3rdparty/VulkanMemoryAllocator)
	set(HEADERS ${HEADERS} ${Vulkan_INCLUDE_DIR}/vulkan/)
endif()

set(SOURCES ${SOURCES} ${SOURCES_SYSTEM} ${SOURCES_VIDEO} ${SOURCES_3RDPARTY})
set(HEADERS ${HEADERS} "resources/shaders/")
set(HEADERS ${HEADERS} ${HEADERS_SYSTEM} ${HEADERS_VIDEO} ${HEADERS_3RDPARTY})
set(VIDEO_FILES ${SOURCES_VIDEO} ${HEADERS_VIDEO})
source_group("system" FILES ${SOURCES_SYSTEM} ${HEADERS_SYSTEM})
source_group("3rdparty" FILES ${SOURCES_3RDPARTY} ${HEADERS_3RDPARTY})
source_group("video" FILES ${VIDEO_FILES})

include_directories(src/engine/video 
	src/engine/video/d3d12
	src/engine/system
	src/engine/math
    src/engine/3rdparty
    src/engine/3rdparty/pugixml
    src/engine/3rdparty/DirectXTK/Inc
    src/engine/3rdparty/DirectXTex)

# TODO: fix C: drive
set(DXC_PATH "C:\\Program Files (x86)\\Windows Kits\\10\\bin\\${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}\\x64\\dxcompiler.dll")
set(DXIL_PATH "C:\\Program Files (x86)\\Windows Kits\\10\\bin\\${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}\\x64\\dxil.dll")
message("DXC_PATH = ${DXC_PATH}")
message("DXIL_PATH = ${DXIL_PATH}")
		
include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("/std:c++latest" _cpp_latest_flag_supported)
if (_cpp_latest_flag_supported)
    add_compile_options("/std:c++latest")
else()
	add_compile_options("/std:c++17")
endif()

include_directories(${HEADERS})

# engine
set(ENGINE_LIBS dxguid dxgi d3d12 d3dcompiler)
if (VK_ENABLE)
	set(ENGINE_LIBS ${ENGINE_LIBS} ${Vulkan_LIBRARY})
endif()
add_library(engine SHARED ${SOURCES})
target_precompile_headers(engine PRIVATE src/engine/system/pch.h)
target_link_libraries(engine ${ENGINE_LIBS})
target_link_libraries(engine yaml-cpp)
target_compile_definitions(engine PUBLIC _DLL_EXPORTS)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")


# engine main
add_executable(engine_main src/examples/engine_main.cpp)
target_link_libraries(engine_main engine)
target_compile_features(engine_main PRIVATE cxx_std_17)
set_property(TARGET engine_main PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/build")

# const buffer example
add_executable(constantbuffers src/examples/constantbuffers.cpp)
target_include_directories(constantbuffers PRIVATE src/examples)
target_link_libraries(constantbuffers engine)
set_property(TARGET constantbuffers PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/build")

# uav barriers example
add_executable(uavbarriers src/examples/uavbarriers.cpp)
target_link_libraries(uavbarriers engine)
set_target_properties(uavbarriers PROPERTIES LINK_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:CONSOLE")
set_property(TARGET uavbarriers PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/build")

# raytracing
if (RAYTRACING_SAMPLE)
	add_executable(raytracing src/examples/raytracing.cpp src/examples/raytracing_utils.cpp)
	target_include_directories(raytracing PRIVATE src/examples)
	target_link_libraries(raytracing engine)
	target_link_libraries(raytracing dxcompiler)
	set_property(TARGET raytracing PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/build")

	# copy dxcompiler.dll for raytracing
	add_custom_command(TARGET raytracing POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${DXC_PATH}"
		$<TARGET_FILE_DIR:raytracing>)

	# copy dxil.dll for raytracing
	add_custom_command(TARGET raytracing POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${DXIL_PATH}"
		$<TARGET_FILE_DIR:raytracing>)
endif()