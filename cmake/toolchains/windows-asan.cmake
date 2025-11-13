set(CMAKE_SYSTEM_NAME Windows)

set(CMAKE_C_COMPILER cl CACHE STRING "" FORCE)
set(CMAKE_CXX_COMPILER cl CACHE STRING "" FORCE)

set(CMAKE_C_FLAGS_INIT "/fsanitize=address /Zi /Od /MDd")
set(CMAKE_CXX_FLAGS_INIT "/fsanitize=address /Zi /Od /MDd")
set(CMAKE_EXE_LINKER_FLAGS_INIT "/INCREMENTAL:NO /fsanitize=address")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "/INCREMENTAL:NO /fsanitize=address")

