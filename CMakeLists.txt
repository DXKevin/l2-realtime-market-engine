cmake_minimum_required(VERSION 3.29.0)
project(L2RealtimeMarketEngine VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)  # .lib 输出也放 bin

# 源文件
aux_source_directory(${PROJECT_SOURCE_DIR}/src SRC_LISTS)


# 👇 改为 STATIC（静态库）
add_library(common STATIC ${SRC_LISTS})

target_include_directories(common
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/third_party/include
)

if(WIN32)
    link_directories(${PROJECT_SOURCE_DIR}/third_party/lib)
    target_link_libraries(common PRIVATE ws2_32 z)
endif()

# 主程序
add_executable(main main.cpp)

if(MINGW)
    target_compile_options(common PRIVATE -Wa,-mbig-obj)
    target_compile_options(main   PRIVATE -Wa,-mbig-obj)
endif()

# 链接 common（现在是 .lib）
target_link_libraries(main PRIVATE common)