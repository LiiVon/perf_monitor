# 找 protobuf 编译器  与 grpc
# ONFIG 是找现代安装方式如vcpkg、apt ，MODULE 是找老式系统路径
find_package(protobuf CONFIG QUIET)
if(NOT protobuf_FOUND)
    find_package(Protobuf REQUIRED)
endif()
find_package(gRPC CONFIG REQUIRED)

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

# 根据找到的包配置链接库
# 判断用“新式目标名”（protobuf::libprotobuf）还是“旧式变量名”（${Protobuf_LIBRARIES}）来链接
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

# 生成的代码路径（build/proto/）
target_include_directories(monitor_proto PUBLIC
${CMAKE_CURRENT_BINARY_DIR})     


# 生成 Protobuf 和 gRPC 代码
# 获取 gRPC 插件的路径
if(TARGET gRPC::grpc_cpp_plugin)
    get_target_property(grpc_cpp_plugin_location gRPC::grpc_cpp_plugin LOCATION)
else()
    set(grpc_cpp_plugin_location ${GRPC_CPP_PLUGIN})
endif()

# 生成 Protobuf 序列化代码  运行 protoc 将 .proto 转换为 .pb.h 和 .pb.cc
protobuf_generate(TARGET monitor_proto LANGUAGE cpp)  

# 生成 gRPC 服务代码 再次运行 protoc，带上 --grpc_out 和 --plugin，生成 .grpc.pb.h 和 .grpc.pb.cc
protobuf_generate(TARGET monitor_proto LANGUAGE grpc GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc PLUGIN "protoc-gen-grpc=${grpc_cpp_plugin_location}")


# [.proto 文件] 
#     --> protoc (编译器) 
#         --> 生成 .pb.cc/.pb.h (数据序列化)
#         --> 生成 .grpc.pb.cc/.grpc.pb.h (RPC接口)
#             --> 编译生成 .o 文件
#                 --> 打包成 libmonitor_proto.a (静态库)