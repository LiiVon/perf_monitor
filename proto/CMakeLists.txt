# ============================================
# Proto 子目录 CMakeLists.txt
# 被顶层 CMake 通过 add_subdirectory(proto) 调用
# ============================================

# 不需要再 project() 和 find_package，顶层已经做了

# proto 文件列表
set(PROTO_FILES
    monitor_info.proto
    cpu_load.proto
    cpu_softirq.proto
    cpu_stat.proto
    mem_info.proto
    net_info.proto
    disk_info.proto
    query_api.proto
)

# 生成 monitor_proto 静态库
add_library(monitor_proto ${PROTO_FILES})

# 链接 Protobuf 和 gRPC（使用顶层已找到的库）
if(TARGET protobuf::libprotobuf)
    target_link_libraries(monitor_proto PUBLIC protobuf::libprotobuf)
else()
    target_link_libraries(monitor_proto PUBLIC ${Protobuf_LIBRARIES})
    target_include_directories(monitor_proto PUBLIC ${Protobuf_INCLUDE_DIRS})
endif()

if(TARGET gRPC::grpc++)
    target_link_libraries(monitor_proto PUBLIC gRPC::grpc gRPC::grpc++)
else()
    target_link_libraries(monitor_proto PUBLIC ${GRPC_LIBRARY} ${GRPCPP_LIBRARY})
endif()

# 生成的代码路径（CMAKE_CURRENT_BINARY_DIR 指向 proto/build）
target_include_directories(monitor_proto PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

# ===== 生成 Protobuf 和 gRPC 代码 =====
# 获取 gRPC 插件路径
if(TARGET gRPC::grpc_cpp_plugin)
    get_target_property(grpc_cpp_plugin_location gRPC::grpc_cpp_plugin LOCATION)
else()
    set(grpc_cpp_plugin_location ${GRPC_CPP_PLUGIN})
endif()

# 生成 .pb.h / .pb.cc
protobuf_generate(TARGET monitor_proto LANGUAGE cpp)

# 生成 .grpc.pb.h / .grpc.pb.cc
protobuf_generate(
    TARGET monitor_proto
    LANGUAGE grpc
    GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc
    PLUGIN "protoc-gen-grpc=${grpc_cpp_plugin_location}"
)

# 输出构建信息
message(STATUS "Proto library configured: monitor_proto")