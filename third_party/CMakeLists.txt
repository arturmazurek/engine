
target_include_directories(Engine PRIVATE eastl/include)
target_link_directories(Engine PRIVATE eastl/lib/win64/debug)
target_link_libraries(Engine EASTL)