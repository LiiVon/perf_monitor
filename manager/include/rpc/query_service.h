#pragma once

#include "query_api.grpc.pb.h"
#include "query_api.pb.h"
#include "query_manager.h"
#include <grpcpp/server_context.h>
#include <grpcpp/support/status.h>
#include <memory>

// 查询历史数据
// proto文件中定义了GrpcQueryService服务，包含GetMonitorInfo方法
// 实现 QueryService（查询服务），处理 Web/前端发来的查询请求（如 QueryPerformance）。内部调用 QueryManager。