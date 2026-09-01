cmake_minimum_required(VERSION 3.10)
project(FSShareClient)

# 使用 C++11 或更高版本（可根据需要调整）
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置编译器选项（可根据需要调整）
# set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/deps/include")

# 添加可执行文件
add_executable(FSShareClient
    src/main.cc
    src/websocket_client.cc
    src/websocket_client.h
    src/simple_thread.cc
    src/simple_thread.h
    src/webrtc_signal_message.cc
    src/webrtc_signal_message.h
    src/webrtc_signal_connection.cc
    src/webrtc_signal_connection.h
    src/webrtc_peer_connection.cc
    src/webrtc_peer_connection.h
)

# 头文件搜索路径（src 目录）
target_include_directories(FSShareClient PRIVATE src)
target_include_directories(FSShareClient PRIVATE include)
target_include_directories(FSShareClient PRIVATE deps/include)

# ============================================
# 依赖库配置（根据实际使用的 websocket 库修改）
# ============================================
set(libwebsockets_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/lib/cmake/libwebsockets")
find_package(libwebsockets REQUIRED)   # 如果提供了 CMake 配置
target_link_libraries(FSShareClient PRIVATE websockets )

# jsoncpp
set(jsoncpp_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/lib/cmake/jsoncpp")
find_package(jsoncpp REQUIRED)   # 如果提供了 CMake 配置
target_link_libraries(FSShareClient PRIVATE JsonCpp::JsonCpp)

# 示例 3：使用 Boost.Asio + Beast（需要 Boost 库）
# find_package(Boost REQUIRED COMPONENTS system)
# target_link_libraries(FSShareClient PRIVATE Boost::system)

# 如果没有任何外部依赖，上面的链接部分可以留空
# 但你的 websocket_client 实现很可能依赖某个库，请按需启用并修改
