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
    "Lab_10/lab10.cpp"
    "Lab_10/lab10_dashboard.cpp"
    "Lab_10/lab10_semaphore.cpp"
)

target_include_directories(lab10 PRIVATE ${CURSES_INCLUDE_DIR})
target_link_libraries(lab10 PRIVATE Threads::Threads ${CURSES_LIBRARIES})

add_executable(lab11
    "Lab_11/lab11.cpp"
)

target_include_directories(lab11 PRIVATE ${CURSES_INCLUDE_DIR})
target_link_libraries(lab11 PRIVATE Threads::Threads ${CURSES_LIBRARIES})

add_executable(lab12
    "Lab_12/lab12.cpp"
)

target_include_directories(lab12 PRIVATE ${CURSES_INCLUDE_DIR})
target_link_libraries(lab12 PRIVATE Threads::Threads ${CURSES_LIBRARIES})

add_executable(lab18
    "Lab_18/lab18.cpp"
)

target_include_directories(lab18 PRIVATE ${CURSES_INCLUDE_DIR})
target_link_libraries(lab18 PRIVATE Threads::Threads ${CURSES_LIBRARIES})

add_executable(lab22_deadlock
    "Lab_22/lab22_Deadlock.cpp"
    "Lab_10/lab10_dashboard.cpp"
)

target_include_directories(lab22_deadlock PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Lab_10"
    ${CURSES_INCLUDE_DIR}
)
target_link_libraries(lab22_deadlock PRIVATE Threads::Threads ${CURSES_LIBRARIES})

add_executable(lab21
    "Lab_21/lab21.cpp"
    "Lab_10/lab10_dashboard.cpp"
)

target_include_directories(lab21 PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Lab_10"
    ${CURSES_INCLUDE_DIR}
)
target_link_libraries(lab21 PRIVATE Threads::Threads ${CURSES_LIBRARIES})

add_executable(lab15
    "Lab_15/lab15.cpp"
    "Lab_10/lab10_dashboard.cpp"
)

target_include_directories(lab15 PRIVATE "Lab_10" ${CURSES_INCLUDE_DIR})
target_link_libraries(lab15 PRIVATE Threads::Threads ${CURSES_LIBRARIES})

add_executable(lab16
    "Lab_16/lab16.cpp"
    "Lab_10/lab10_dashboard.cpp"
)

target_include_directories(lab16 PRIVATE
    ${CURSES_INCLUDE_DIR}
    "${CMAKE_CURRENT_SOURCE_DIR}/Lab_10"
)
target_link_libraries(lab16 PRIVATE Threads::Threads ${CURSES_LIBRARIES})

add_executable(lab13
    "Lab_13/lab13.cpp"
)

target_include_directories(lab13 PRIVATE ${CURSES_INCLUDE_DIR})
target_link_libraries(lab13 PRIVATE ${CURSES_LIBRARIES})

add_executable(lab14
    "Lab_14/lab14.cpp"
    "Lab_10/lab10_dashboard.cpp"
)

target_include_directories(lab14 PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Lab_10"
    ${CURSES_INCLUDE_DIR}
)
target_link_libraries(lab14 PRIVATE Threads::Threads ${CURSES_LIBRARIES})

add_executable(lab20
    "Lab_20/lab20.cpp"
    "Lab_10/lab10_dashboard.cpp"
)

target_include_directories(lab20 PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Lab_10"
    ${CURSES_INCLUDE_DIR}
)
target_link_libraries(lab20 PRIVATE Threads::Threads ${CURSES_LIBRARIES})

add_executable(lab23
    "Lab_23/lab23.cpp"
)

target_link_libraries(lab23 PRIVATE Threads::Threads)
