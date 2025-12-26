cmake_minimum_required(VERSION 3.29.0)
project(L2RealtimeMarketEngine VERSION 1.0.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)

aux_source_directory(${PROJECT_SOURCE_DIR}/src SRC_LISTS)

add_library(common 
SHARED 
${SRC_LISTS})

target_include_directories(common 
PUBLIC 
${PROJECT_SOURCE_DIR}/include
${PROJECT_SOURCE_DIR}/third_party
)

add_executable(main main.cpp)

target_include_directories(main
PUBLIC 
${PROJECT_SOURCE_DIR}/include
${PROJECT_SOURCE_DIR}/third_party
)

target_link_libraries(main 
PUBLIC 
common
)   
