cmake_minimum_required(VERSION 3.16)
project(reactor_sim CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Same float-safety warnings as the firmware build. Doctest itself is a system header.
set(WARNINGS -Wall -Wextra -Wdouble-promotion -Wfloat-conversion -Wshadow)

# MinGW: link the C++ runtime statically so the .exe files run without MinGW on PATH.
if(MINGW)
  add_link_options(-static)
endif()

file(GLOB REACTOR_SOURCES CONFIGURE_DEPENDS lib/reactor/src/*.cpp)
add_library(reactor STATIC ${REACTOR_SOURCES})
target_include_directories(reactor PUBLIC lib/reactor/src)
target_compile_options(reactor PRIVATE ${WARNINGS})

add_executable(sim_cli desktop/sim_cli.cpp)
target_link_libraries(sim_cli PRIVATE reactor)
target_compile_options(sim_cli PRIVATE ${WARNINGS})

file(GLOB TEST_SOURCES CONFIGURE_DEPENDS tests/*.cpp)
add_executable(unit_tests ${TEST_SOURCES})
target_include_directories(unit_tests SYSTEM PRIVATE third_party)
target_link_libraries(unit_tests PRIVATE reactor)
# Tests compute references in double on purpose, so the float-only flags stay off here.
target_compile_options(unit_tests PRIVATE -Wall -Wextra -Wshadow)

enable_testing()
add_test(NAME unit_tests COMMAND unit_tests)
