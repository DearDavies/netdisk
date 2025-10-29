cmake_minimum_required(VERSION 3.31)
project(netdisk C)

set(CMAKE_C_STANDARD 11)

# 自动查找 client 文件夹下所有的 .c 文件
file(GLOB CLIENT_SOURCES "client/*.c")
file(GLOB CLIENT_MODULES "client/modules/*.c")

# 自动查找 server 文件夹下所有的 .c 文件
file(GLOB SERVER_SOURCES "server/*.c")
file(GLOB SERVER_MODULES "server/modules/*.c")

# 使用这些源文件创建可执行文件
add_executable(client_app ${CLIENT_SOURCES} ${CLIENT_MODULES})
add_executable(server_app ${SERVER_SOURCES} ${SERVER_MODULES})

# 1. 查找系统上的线程库
#    REQUIRED 参数表示如果找不到线程库，配置过程将失败
find_package(Threads REQUIRED)

# 2. 将找到的线程库链接到你的可执行文件
#    Threads::Threads 是一个 CMake 目标，它包含了所有需要的信息
#    （比如头文件路径、库文件路径、需要链接的库名 -lpthread 等）
target_link_libraries(server_app PRIVATE Threads::Threads)

# 3. 查找 MySQL 和 OpenSSL 库
find_package(PkgConfig REQUIRED)
pkg_check_modules(MYSQL REQUIRED mysqlclient)
pkg_check_modules(OPENSSL REQUIRED openssl)

# 4. 链接 MySQL 和 OpenSSL 库到服务端
target_include_directories(server_app PRIVATE ${MYSQL_INCLUDE_DIRS})
target_link_libraries(server_app PRIVATE ${MYSQL_LIBRARIES})

# 5. 链接 OpenSSL 到客户端（用于 SHA512）
target_link_libraries(client_app PRIVATE ${OPENSSL_LIBRARIES})
target_include_directories(client_app PRIVATE ${OPENSSL_INCLUDE_DIRS})
