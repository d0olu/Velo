cmake_minimum_required(VERSION 3.16)
project(elberr VERSION 2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(CURL REQUIRED)
find_package(Threads REQUIRED)

option(ELBERR_ENABLE_HIP "Enable AMD ROCm/HIP GPU acceleration" OFF)

set(ELBERR_SOURCES
    src/formula.cpp
    src/epistemic.cpp
    src/belief_base.cpp
    src/reasoner.cpp
    src/russian_grammar.cpp
    src/language_model.cpp
    src/web_fetcher.cpp
    src/autonomous_explorer.cpp
    src/gpu_engine.cpp
    src/persistence.cpp
    src/will_engine.cpp
    src/code_engine.cpp
    src/agent.cpp
    src/chat_server.cpp
    src/elberr.cpp
    src/main.cpp
)

add_executable(elberr ${ELBERR_SOURCES})

target_include_directories(elberr PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CURL_INCLUDE_DIRS}
)

target_link_libraries(elberr PRIVATE
    ${CURL_LIBRARIES}
    Threads::Threads
)

if(ELBERR_ENABLE_HIP)
    find_package(hip REQUIRED)
    target_compile_definitions(elberr PRIVATE ELBERR_HIP=1)
    target_link_libraries(elberr PRIVATE hip::device)
endif()

target_compile_options(elberr PRIVATE -Wall -Wextra -O2)
