
// 查询历史数据
// proto文件中定义了GrpcQueryService服务，包含GetMonitorInfo方法
// 实现 QueryService（查询服务），处理 Web/前端发来的查询请求（如 QueryPerformance）。内部调用 QueryManager。

#pragma once

#include <grpcpp/server_context.h>
#include <grpcpp/support/status.h>

#include <memory>

#include "query_api.grpc.pb.h"
#include "query_api.pb.h"
#include "query_manager.h"

namespace monitor
{

    // gRPC 查询服务实现类
    class QueryServiceImpl : public monitor::proto::QueryService::Service
    {
    public:
        explicit QueryServiceImpl(QueryManager *query_manager);
        virtual ~QueryServiceImpl() = default;

        // 时间段性能数据查询
        ::grpc::Status QueryPerformance(
            ::grpc::ServerContext *context,
            const ::monitor::proto::QueryPerformanceRequest *request,
            ::monitor::proto::QueryPerformanceResponse *response) override;

        // 变化率趋势查询
        ::grpc::Status
        QueryTrend(::grpc::ServerContext *context,
                   const ::monitor::proto::QueryTrendRequest *request,
                   ::monitor::proto::QueryTrendResponse *response) override;

        // 异常数据查询
        ::grpc::Status
        QueryAnomaly(::grpc::ServerContext *context,
                     const ::monitor::proto::QueryAnomalyRequest *request,
                     ::monitor::proto::QueryAnomalyResponse *response) override;

        // 评分排序查询
        ::grpc::Status
        QueryScoreRank(::grpc::ServerContext *context,
                       const ::monitor::proto::QueryScoreRankRequest *request,
                       ::monitor::proto::QueryScoreRankResponse *response) override;

        // 最新评分查询
        ::grpc::Status QueryLatestScore(
            ::grpc::ServerContext *context,
            const ::monitor::proto::QueryLatestScoreRequest *request,
            ::monitor::proto::QueryLatestScoreResponse *response) override;

        // 网络详细数据查询
        ::grpc::Status
        QueryNetDetail(::grpc::ServerContext *context,
                       const ::monitor::proto::QueryDetailRequest *request,
                       ::monitor::proto::QueryNetDetailResponse *response) override;

        // 磁盘详细数据查询
        ::grpc::Status
        QueryDiskDetail(::grpc::ServerContext *context,
                        const ::monitor::proto::QueryDetailRequest *request,
                        ::monitor::proto::QueryDiskDetailResponse *response) override;

        // 内存详细数据查询
        ::grpc::Status
        QueryMemDetail(::grpc::ServerContext *context,
                       const ::monitor::proto::QueryDetailRequest *request,
                       ::monitor::proto::QueryMemDetailResponse *response) override;

        // 软中断详细数据查询
        ::grpc::Status QuerySoftIrqDetail(
            ::grpc::ServerContext *context,
            const ::monitor::proto::QueryDetailRequest *request,
            ::monitor::proto::QuerySoftIrqDetailResponse *response) override;

    private:
        // 转换时间范围
        TimeRange ConvertTimeRange(const ::monitor::proto::TimeRange &proto_range);

        // 转换时间点到protobuf Timestamp
        void SetTimestamp(::google::protobuf::Timestamp *ts,
                          const std::chrono::system_clock::time_point &tp);

        QueryManager *query_manager_;
    };

} // namespace monitor

/*
前端 (Qt UI / Web)
    │
    │ gRPC 请求 (QueryPerformance, QueryTrend, ...)
    ▼
QueryServiceImpl (gRPC 服务实现)
    │
    │ 调用 query_manager_->QueryPerformance(...)
    ▼
QueryManager (MySQL 查询封装)
    │
    │ 执行 SQL: SELECT ... FROM server_performance WHERE ...
    ▼
MySQL 数据库 (monitor_db)
    │
    │ 返回查询结果 (行数据)
    ▼
QueryManager 将结果转换为 PerformanceRecord / NetDetailRecord 等
    │
    ▼
QueryServiceImpl 将结构体填充到 protobuf 响应消息
    │
    ▼
前端收到响应，解析并显示 (QChart 画曲线、QTableWidget 展示列表)
*/
