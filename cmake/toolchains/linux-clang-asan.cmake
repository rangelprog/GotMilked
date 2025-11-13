set(CMAKE_SYSTEM_NAME Linux)

set(CMAKE_C_COMPILER clang CACHE STRING "" FORCE)
set(CMAKE_CXX_COMPILER clang++ CACHE STRING "" FORCE)

set(CMAKE_C_FLAGS_INIT "-fsanitize=address -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS_INIT "-fsanitize=address -fno-omit-frame-pointer")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-fsanitize=address")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-fsanitize=address")

