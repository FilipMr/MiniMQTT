cmake_minimum_required(VERSION 3.20)

project(MiniMQTT
        VERSION 0.0.1)

add_executable(${PROJECT_NAME}
    src/main.c
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${PROJECT_SOURCE_DIR}/inc
)

target_compile_options(${PROJECT_NAME}
    PRIVATE
        -Wall
)