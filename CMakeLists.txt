cmake_minimum_required(VERSION 3.29.0)
project(L2RealtimeMarketEngine VERSION 1.0.0 LANGUAGES CXX)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 输出目录设置
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)

# 自动收集 src/ 下的所有源文件
aux_source_directory(${PROJECT_SOURCE_DIR}/src SRC_LISTS)

# 创建共享库 common
add_library(common SHARED ${SRC_LISTS})

# 设置 common 库的头文件路径
target_include_directories(common PUBLIC ${PROJECT_SOURCE_DIR}/include)

# 添加主程序 executable
add_executable(L2RealtimeMarketEngine main.cpp)

# 设置主程序的头文件路径
target_include_directories(L2RealtimeMarketEngine PUBLIC ${PROJECT_SOURCE_DIR}/include)

# 主程序链接 common 库
target_link_libraries(L2RealtimeMarketEngine PUBLIC common)

