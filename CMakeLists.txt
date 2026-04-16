cmake_minimum_required(VERSION 4.2)
project(Extra_Credit)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CURSES_NEED_NCURSES TRUE)
find_package(Curses REQUIRED)
find_package(Threads REQUIRED)

add_executable(Extra_Credit main.cpp)

add_executable(lab10
    "Phase 10/lab10.cpp"
    "Phase 10/lab10_dashboard.cpp"
    "Phase 10/lab10_semaphore.cpp"
)

target_include_directories(lab10 PRIVATE ${CURSES_INCLUDE_DIR})
target_link_libraries(lab10 PRIVATE Threads::Threads ${CURSES_LIBRARIES})

add_executable(lab11
    "Phase 11/lab11.cpp"
    "Phase 10/lab10_dashboard.cpp"
)

target_include_directories(lab11 PRIVATE ${CURSES_INCLUDE_DIR} "Phase 10")
target_link_libraries(lab11 PRIVATE Threads::Threads ${CURSES_LIBRARIES})

add_executable(lab12
    "Phase 12/lab12.cpp"
    "Phase 12/lab12_scheduler.cpp"
    "Phase 12/lab12_semaphore.cpp"
)

target_link_libraries(lab12 PRIVATE Threads::Threads)
