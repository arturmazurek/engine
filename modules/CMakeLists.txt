
find_package(Vulkan REQUIRED)

set(GLFW_INCLUDE_DIRS
    D:/Projects/glfw-3.3.8.bin.WIN64/include
)

set(GLFW_LIBRARY_DIRS
    D:/Projects/glfw-3.3.8.bin.WIN64/lib-vc2022
)

set(ENGINE_SOURCES
	include/ArraySize.h
	include/Constants.h
	include/DebugBreak.h
    include/Engine.h
    include/EngineContext.h
	include/Log.h
	
	src/ArraySize.cpp
	src/Constants.cpp
	src/DebugBreak.cpp
    src/Engine.cpp
    src/EngineContext.cpp
	src/Log.cpp
	src/platform/windows/WindowsDebugBreak.cpp
	
	main.cpp
)

add_executable(Engine ${ENGINE_SOURCES})

target_include_directories(Engine PRIVATE ${Vulkan_INCLUDE_DIRS})
target_include_directories(Engine PRIVATE ${GLFW_INCLUDE_DIRS})
target_include_directories(Engine PRIVATE include)

target_link_directories(Engine PRIVATE ${GLFW_LIBRARY_DIRS})
target_link_libraries(Engine glfw3)
target_link_libraries(Engine ${Vulkan_LIBRARIES})