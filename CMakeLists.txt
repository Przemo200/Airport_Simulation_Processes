cmake_minimum_required(VERSION 3.10)
project(AirportUltimate CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -g)

find_package(Curses REQUIRED)
find_package(Threads REQUIRED)

add_executable(airport_main airport_main.cpp shared_state.h)

target_link_libraries(airport_maincmake_minimum_required(VERSION 3.10)
project(AirportUltimate CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -g)

find_package(Curses REQUIRED)
find_package(Threads REQUIRED)

add_executable(airport_main airport_main.cpp shared_state.h)

target_link_libraries(airport_main
        PRIVATE
        ${CURSES_LIBRARIES}
        Threads::Threads
        rt
)

add_executable(plane_main plane_main.cpp shared_state.h)

target_link_libraries(plane_main
        PRIVATE
        Threads::Threads
        rt
)
        PRIVATE
        ${CURSES_LIBRARIES}
        Threads::Threads
        rt
)

add_executable(plane_main plane_main.cpp shared_state.h)

target_link_libraries(plane_main
        PRIVATE
        Threads::Threads
        rt
)