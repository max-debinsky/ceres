cmake_minimum_required(VERSION 3.20)
project(ceres_emulator CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(blit EXCLUDE_FROM_ALL)

add_executable(ceres_emulator
    src/main.cpp
    src/cpu.cpp
    src/memory.cpp
    src/display.cpp
)

target_include_directories(ceres_emulator PRIVATE src)
target_link_libraries(ceres_emulator PRIVATE blit)

if(MSVC)
    target_compile_options(ceres_emulator PRIVATE /W4)
else()
    target_compile_options(ceres_emulator PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- Tools ---
add_executable(gen_test_rom tools/gen_test_rom.cpp)
target_compile_options(gen_test_rom PRIVATE
    $<IF:$<CXX_COMPILER_ID:MSVC>,/W4,-Wall -Wextra>)
