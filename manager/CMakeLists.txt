# # Manager - 管理者服务器
# # 部署在管理端，接收工作者推送的数据、存储到数据库、提供查询服务


# # MySQL 支持选项
# option(ENABLE_MYSQL "Enable MySQL support for data persistence" ON)

# set(MANAGER_SOURCES
#     src/main.cpp
#     src/host_manager.cpp
#     src/query_manager.cpp
#     src/rpc/grpc_server.cpp
#     src/rpc/query_service.cpp
# )

# # 可执行文件: manager (管理者服务器)
# add_executable(manager ${MANAGER_SOURCES})

# target_include_directories(manager PUBLIC
#     ${PROJECT_SOURCE_DIR}/manager/include
# )

# # 基础链接库
# set(MANAGER_LIBS monitor_proto gRPC::grpc++)

# # MySQL 依赖（可选）
# if(ENABLE_MYSQL)
#     find_package(PkgConfig QUIET)
#     if(PkgConfig_FOUND)
#         pkg_check_modules(MYSQL mysqlclient)
#     endif()
    
#     if(MYSQL_FOUND)
#         message(STATUS "MySQL support enabled")
#         target_compile_definitions(manager PRIVATE ENABLE_MYSQL=1)
#         target_include_directories(manager PUBLIC ${MYSQL_INCLUDE_DIRS})
#         list(APPEND MANAGER_LIBS ${MYSQL_LIBRARIES})
#     else()
#         message(WARNING "MySQL not found, disabling MySQL support")
#     endif()
# endif()

# target_link_libraries(manager PUBLIC ${MANAGER_LIBS})
