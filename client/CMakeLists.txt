cmake_minimum_required(VERSION 3.10)
project(FSShareClient)

# 使用 C++11 或更高版本（可根据需要调整）
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#set(CMAKE_BUILD_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/../../out/Debug")
# 设置编译器选项（可根据需要调整）
# set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/deps/include")

# 添加可执行文件
add_executable(FSShareClient
    src/main.cc
    src/websocket_client.cc
    src/websocket_client.h
    src/simple_thread.cc
    src/simple_thread.h
#    src/webrtc_signal_message.cc
#    src/webrtc_signal_message.h
#    src/webrtc_signal_connection.cc
#    src/webrtc_signal_connection.h
#    src/webrtc_peer_connection.cc
#    src/webrtc_peer_connection.h
)

# 头文件搜索路径（src 目录）
target_include_directories(FSShareClient PRIVATE src)
target_include_directories(FSShareClient PRIVATE include)
target_include_directories(FSShareClient PRIVATE deps/include)


target_link_directories(FSShareClient PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/deps/lib)


# ============================================
# 依赖库配置（根据实际使用的 websocket 库修改）
# ============================================
set(libwebsockets_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/lib/cmake/libwebsockets")
find_package(libwebsockets REQUIRED)   # 如果提供了 CMake 配置
target_link_libraries(FSShareClient PRIVATE websockets )

# jsoncpp
#set(jsoncpp_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/lib/cmake/jsoncpp")
#find_package(jsoncpp REQUIRED)   # 如果提供了 CMake 配置
#target_link_libraries(FSShareClient PRIVATE JsonCpp::JsonCpp)



# 示例 3：使用 Boost.Asio + Beast（需要 Boost 库）
# find_package(Boost REQUIRED COMPONENTS system)
# target_link_libraries(FSShareClient PRIVATE Boost::system)

# 如果没有任何外部依赖，上面的链接部分可以留空
# 但你的 websocket_client 实现很可能依赖某个库，请按需启用并修改

# ============================================
# sharertc SDK（外部已编译好的动态库）
# ============================================
# 外部指定的动态库路径。链接和运行时都使用这个路径。
set(SHARERTC_SDK_PATH
    "${CMAKE_CURRENT_SOURCE_DIR}/../../out/Debug/libsharertc_sdk.dylib")

add_library(sharertc_sdk SHARED IMPORTED)
set_target_properties(sharertc_sdk PROPERTIES
    IMPORTED_LOCATION "${SHARERTC_SDK_PATH}"
)

target_link_libraries(FSShareClient PRIVATE sharertc_sdk)

if(APPLE)
    get_filename_component(SHARERTC_SDK_DIR "${SHARERTC_SDK_PATH}" DIRECTORY)

    # SDK 的 install_name 已经是 @rpath/libsharertc_sdk.dylib（见
    # sharertc/BUILD.gn），这里只需提供 rpath：
    #   - SDK 所在目录：运行时加载链接时指定的同一个外部动态库；
    #   - @loader_path：库被拷贝到可执行文件目录时也能找到。
    set_target_properties(FSShareClient PROPERTIES
        BUILD_RPATH "${SHARERTC_SDK_DIR};@loader_path"
    )

    # 同时拷贝一份到可执行文件目录，配合 @loader_path 使用。
    add_custom_command(TARGET FSShareClient POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:sharertc_sdk>"
            "$<TARGET_FILE_DIR:FSShareClient>"
        COMMENT "Copy libsharertc_sdk.dylib next to FSShareClient"
    )
endif()