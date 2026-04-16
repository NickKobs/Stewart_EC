cmake_minimum_required(VERSION 3.20)
project(Extra_Credit)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CURSES_NEED_NCURSES TRUE)
find_package(Curses REQUIRED)
find_package(Threads REQUIRED)

add_executable(Extra_Credit main.cpp)
target_include_directories(Extra_Credit PRIVATE ${CURSES_INCLUDE_DIR})
target_link_libraries(Extra_Credit PRIVATE ${CURSES_LIBRARIES})

add_executable(lab10
    "Phase 10/lab10.cpp"
    "Phase 10/lab10_dashboard.cpp"
    "Phase 10/lab10_semaphore.cpp"
)

target_include_directories(lab10 PRIVATE ${CURSES_INCLUDE_DIR})
target_link_libraries(lab10 PRIVATE Threads::Threads ${CURSES_LIBRARIES})

add_executable(lab11
    "Phase 11/lab11.cpp"
)

target_include_directories(lab11 PRIVATE ${CURSES_INCLUDE_DIR})
target_link_libraries(lab11 PRIVATE Threads::Threads ${CURSES_LIBRARIES})

add_executable(lab12
    "Phase 12/lab12.cpp"
)

target_include_directories(lab12 PRIVATE ${CURSES_INCLUDE_DIR})
target_link_libraries(lab12 PRIVATE Threads::Threads ${CURSES_LIBRARIES})

add_executable(lab18
    "Phase 18/lab18.cpp"
)

target_include_directories(lab18 PRIVATE ${CURSES_INCLUDE_DIR})
target_link_libraries(lab18 PRIVATE Threads::Threads ${CURSES_LIBRARIES})

add_executable(lab15
    "Phase 15/lab15.cpp"
    "Phase 10/lab10_dashboard.cpp"
)

target_include_directories(lab15 PRIVATE "Phase 10" ${CURSES_INCLUDE_DIR})
target_link_libraries(lab15 PRIVATE Threads::Threads ${CURSES_LIBRARIES})

add_executable(lab16
    "Phase 16/lab16.cpp"
    "Phase 10/lab10_dashboard.cpp"
)

target_include_directories(lab16 PRIVATE
    ${CURSES_INCLUDE_DIR}
    "${CMAKE_CURRENT_SOURCE_DIR}/Phase 10"
)
target_link_libraries(lab16 PRIVATE Threads::Threads ${CURSES_LIBRARIES})

add_executable(lab13
    "Phase 13/lab13.cpp"
)

target_include_directories(lab13 PRIVATE ${CURSES_INCLUDE_DIR})
target_link_libraries(lab13 PRIVATE ${CURSES_LIBRARIES})

add_executable(lab14
    "Phase 14/lab14.cpp"
    "Phase 10/lab10_dashboard.cpp"
)

target_include_directories(lab14 PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Phase 10"
    ${CURSES_INCLUDE_DIR}
)
target_link_libraries(lab14 PRIVATE Threads::Threads ${CURSES_LIBRARIES})
