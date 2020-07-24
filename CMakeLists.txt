cmake_minimum_required (VERSION 3.16)
 
project (x12lib)

option(VK_ENABLE "Enable vulkan renderer" OFF)

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

set(SOURCES ${SOURCES} ${SOURCES_SYSTEM} ${SOURCES_VIDEO} ${SOURCES_3RDPARTY})
set(HEADERS ${HEADERS} ${HEADERS_SYSTEM} ${HEADERS_VIDEO} ${HEADERS_3RDPARTY})

source_group("system" FILES ${SOURCES_SYSTEM} ${HEADERS_SYSTEM})
source_group("3rdparty" FILES ${SOURCES_3RDPARTY} ${HEADERS_3RDPARTY})
source_group("video" FILES ${SOURCES_VIDEO} ${HEADERS_VIDEO})

include_directories(src/engine/video 
	src/engine/video/d3d12
	src/engine/system
	src/engine/math
    src/engine/3rdparty
    src/engine/3rdparty/pugixml
    src/engine/3rdparty/DirectXTK/Inc
    src/engine/3rdparty/DirectXTex)
	
# Vulkan
if (VK_ENABLE)
	add_definitions(-DVK_ENABLE=1)
	file(GLOB SOURCES_VK src/engine/video/vk/*.cpp)
	file(GLOB HEADERS_VK src/engine/video/vk/*.h)
	set(SOURCES ${SOURCES} ${SOURCES_VK})
	set(HEADERS ${HEADERS} ${HEADERS_VK})
	include_directories(src/engine/video/vk)
endif()
	
#message("SOURCES = ${SOURCES}")
#message("HEADERS = ${HEADERS}")
#message("CMAKE_CURRENT_SOURCE_DIR = ${CMAKE_CURRENT_SOURCE_DIR}")

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("/std:c++latest" _cpp_latest_flag_supported)
if (_cpp_latest_flag_supported)
    add_compile_options("/std:c++latest")
else()
	add_compile_options("/std:c++17")
endif()

include_directories(${HEADERS})

# engine
add_library(engine SHARED ${SOURCES})
target_precompile_headers(engine PRIVATE src/engine/system/pch.h)
target_link_libraries(engine dxguid dxgi d3d12 d3dcompiler)
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

# uav_barriers example
add_executable(uavbarriers src/examples/uavbarriers.cpp)
target_link_libraries(uavbarriers engine)
set_target_properties(uavbarriers PROPERTIES LINK_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:CONSOLE")
set_property(TARGET uavbarriers PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/build")
