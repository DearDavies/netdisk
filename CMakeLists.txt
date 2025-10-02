cmake_minimum_required(VERSION 3.31)
project(multithreadServer C)

set(CMAKE_C_STANDARD 11)

# 自动查找 client 文件夹下所有的 .cpp 文件
file(GLOB CLIENT_SOURCES "client/*.c")
# 自动查找 server 文件夹下所有的 .cpp 文件
file(GLOB SERVER_SOURCES "server/*.c")

# 使用这些源文件创建可执行文件
add_executable(client_app ${CLIENT_SOURCES})
add_executable(server_app ${SERVER_SOURCES})

# 1. 查找系统上的线程库
#    REQUIRED 参数表示如果找不到线程库，配置过程将失败
find_package(Threads REQUIRED)

# 2. 将找到的线程库链接到你的可执行文件
#    Threads::Threads 是一个 CMake 目标，它包含了所有需要的信息
#    （比如头文件路径、库文件路径、需要链接的库名 -lpthread 等）
target_link_libraries(server_app PRIVATE Threads::Threads)
