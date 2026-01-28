cmake_minimum_required(VERSION 3.20)
project(MiniMQTT VERSION 0.0.1 LANGUAGES C)

add_executable(MiniMQTT
    src/main.c
    src/cJSON.c
)

add_executable(client_tcp
    src/client_tcp.c
    src/cJSON.c
)

target_include_directories(MiniMQTT PRIVATE ${PROJECT_SOURCE_DIR}/inc)
target_include_directories(client_tcp PRIVATE ${PROJECT_SOURCE_DIR}/inc)

target_compile_options(MiniMQTT PRIVATE -Wall)
target_compile_options(client_tcp PRIVATE -Wall)
