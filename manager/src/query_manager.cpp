#include "query_manager.h"
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace monitor
{

QueryManager::QueryManager() = default;

QueryManager::~QueryManager() { Close(); }

bool QueryManager::Init(const std::string &host, const std::string &user,
                        const std::string &password,
                        const std::string &database)
{
#ifdef ENABLE_MYSQL
  std::lock_guard<std::mutex> lock(mtx_);
  if (initialized_)
  {
    return true;
  }

  conn_ = mysql_init(nullptr);
  if (!conn_)
  {
    std::cerr << "QueryManager: mysql_init failed" << std::endl;
    return false;
  }

  if (!mysql_real_connect(conn_, host.c_str(), user.c_str(), password.c_str(),
                          database.c_str(), 0, nullptr, 0))
  {
    std::cerr << "QueryManager: mysql_real_connect failed: "
              << mysql_error(conn_) << std::endl;
    mysql_close(conn_);
    conn_ = nullptr;
    return false;
  }

  // 设置字符集
  mysql_set_character_set(conn_, "utf8mb4");
  initialized_ = true;
  std::cout << "QueryManager: MySQL connection initialized" << std::endl;
  return true;
#else
  (void)host;
  (void)user;
  (void)password;
  (void)database;
  std::cerr << "QueryManager: MySQL support not enabled" << std::endl;
  return false;
#endif
}

void QueryManager::Close()
{
#ifdef ENABLE_MYSQL
  std::lock_guard<std::mutex> lock(mtx_);
  if (conn_)
  {
    mysql_close(conn_);
    conn_ = nullptr;
  }
  initialized_ = false;
#endif
}

bool QueryManager::ValidateTimeRange(const TimeRange &range) const
{
  return range.start_time <= range.end_time;
}

std::string
QueryManager::FormatTime(const std::chrono::system_clock::time_point &tp) const
{
  std::time_t t = std::chrono::system_clock::to_time_t(tp);
  std::tm tm_time;
  localtime_r(&t, &tm_time);
  char buf[32];
  std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_time);
  return std::string(buf);
}

std::chrono::system_clock::time_point
QueryManager::ParseTime(const char *str) const
{
  std::tm tm = {};
  std::istringstream ss(str);
  ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
  return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

int QueryManager::GetTotalCount(const std::string &count_sql)
{
#ifdef ENABLE_MYSQL
  if (mysql_query(conn_, count_sql.c_str()) != 0)
  {
    std::cerr << "QueryManager: count query failed: " << mysql_error(conn_)
              << std::endl;
    return 0;
  }

  MYSQL_RES *result = mysql_store_result(conn_);
  if (!result)
  {
    return 0;
  }

  int count = 0;
  MYSQL_ROW row = mysql_fetch_row(result);
  if (row && row[0])
  {
    count = std::atoi(row[0]);
  }
  mysql_free_result(result);
  return count;
#else
  (void)count_sql;
  return 0;
#endif
}

std::vector<PerformanceRecord>
QueryManager::QueryPerformance(const std::string &server_name,
                               const TimeRange &time_range, int page,
                               int page_size, int *total_count)
{
  std::vector<PerformanceRecord> records;

#ifdef ENABLE_MYSQL
  std::lock_guard<std::mutex> lock(mtx_);
  if (!initialized_ || !conn_)
  {
    return records;
  }

  // 验证参数
  if (!ValidateTimeRange(time_range))
  {
    std::cerr << "QueryManager: Invalid time range" << std::endl;
    return records;
  }
  if (page < 1)
    page = 1;
  if (page_size < 1)
    page_size = 100;

  std::string start_time = FormatTime(time_range.start_time);
  std::string end_time = FormatTime(time_range.end_time);

  // 获取总数
  std::ostringstream count_sql;
  count_sql << "SELECT COUNT(*) FROM server_performance WHERE server_name='"
            << server_name << "' AND timestamp BETWEEN '" << start_time
            << "' AND '" << end_time << "'";
  if (total_count)
  {
    *total_count = GetTotalCount(count_sql.str());
  }

  // 查询数据
  int offset = (page - 1) * page_size;
  std::ostringstream sql;
  sql << "SELECT server_name, timestamp, cpu_percent, usr_percent, "
         "system_percent, nice_percent, idle_percent, io_wait_percent, "
         "irq_percent, soft_irq_percent, load_avg_1, load_avg_5, load_avg_15, "
         "mem_used_percent, total, free, avail, disk_util_percent, "
         "send_rate, rcv_rate, score, cpu_percent_rate, mem_used_percent_rate, "
         "disk_util_percent_rate, load_avg_1_rate, send_rate_rate, "
         "rcv_rate_rate "
         "FROM server_performance WHERE server_name='"
      << server_name << "' AND timestamp BETWEEN '" << start_time << "' AND '"
      << end_time << "' ORDER BY timestamp DESC LIMIT " << page_size
      << " OFFSET " << offset;

  if (mysql_query(conn_, sql.str().c_str()) != 0)
  {
    std::cerr << "QueryManager: query failed: " << mysql_error(conn_)
              << std::endl;
    return records;
  }

  MYSQL_RES *result = mysql_store_result(conn_);
  if (!result)
  {
    return records;
  }

  MYSQL_ROW row;
  while ((row = mysql_fetch_row(result)))
  {
    PerformanceRecord rec;
    int i = 0;
    rec.server_name = row[i++] ? row[i - 1] : "";
    rec.timestamp =
        row[i] ? ParseTime(row[i]) : std::chrono::system_clock::now();
    i++;
    rec.cpu_percent = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.usr_percent = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.system_percent = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.nice_percent = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.idle_percent = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.io_wait_percent = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.irq_percent = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.soft_irq_percent = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.load_avg_1 = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.load_avg_5 = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.load_avg_15 = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.mem_used_percent = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.mem_total = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.mem_free = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.mem_avail = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.disk_util_percent = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.send_rate = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.rcv_rate = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.score = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.cpu_percent_rate = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.mem_used_percent_rate = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.disk_util_percent_rate = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.load_avg_1_rate = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.send_rate_rate = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.rcv_rate_rate = row[i] ? std::atof(row[i]) : 0;
    records.push_back(rec);
  }
  mysql_free_result(result);
#else
  (void)server_name;
  (void)time_range;
  (void)page;
  (void)page_size;
  (void)total_count;
#endif

  return records;
}

std::vector<PerformanceRecord>
QueryManager::QueryTrend(const std::string &server_name,
                         const TimeRange &time_range, int interval_seconds)
{
  std::vector<PerformanceRecord> records;

#ifdef ENABLE_MYSQL
  std::lock_guard<std::mutex> lock(mtx_);
  if (!initialized_ || !conn_)
  {
    return records;
  }

  if (!ValidateTimeRange(time_range))
  {
    std::cerr << "QueryManager: Invalid time range" << std::endl;
    return records;
  }

  std::string start_time = FormatTime(time_range.start_time);
  std::string end_time = FormatTime(time_range.end_time);

  std::ostringstream sql;
  if (interval_seconds > 0)
  {
    // 带聚合的查询
    sql << "SELECT server_name, "
           "FROM_UNIXTIME(FLOOR(UNIX_TIMESTAMP(timestamp) / "
        << interval_seconds << ") * " << interval_seconds
        << ") as time_bucket, "
           "AVG(cpu_percent) as cpu_percent, "
           "AVG(usr_percent) as usr_percent, "
           "AVG(system_percent) as system_percent, "
           "AVG(io_wait_percent) as io_wait_percent, "
           "AVG(load_avg_1) as load_avg_1, "
           "AVG(load_avg_5) as load_avg_5, "
           "AVG(load_avg_15) as load_avg_15, "
           "AVG(mem_used_percent) as mem_used_percent, "
           "AVG(disk_util_percent) as disk_util_percent, "
           "AVG(send_rate) as send_rate, "
           "AVG(rcv_rate) as rcv_rate, "
           "AVG(score) as score, "
           "AVG(cpu_percent_rate) as cpu_percent_rate, "
           "AVG(mem_used_percent_rate) as mem_used_percent_rate, "
           "AVG(disk_util_percent_rate) as disk_util_percent_rate, "
           "AVG(load_avg_1_rate) as load_avg_1_rate "
           "FROM server_performance WHERE server_name='"
        << server_name << "' AND timestamp BETWEEN '" << start_time << "' AND '"
        << end_time
        << "' GROUP BY server_name, time_bucket ORDER BY time_bucket";
  }
  else
  {
    // 不聚合，直接查询
    sql << "SELECT server_name, timestamp, cpu_percent, usr_percent, "
           "system_percent, io_wait_percent, load_avg_1, load_avg_5, "
           "load_avg_15, mem_used_percent, disk_util_percent, send_rate, "
           "rcv_rate, score, cpu_percent_rate, mem_used_percent_rate, "
           "disk_util_percent_rate, load_avg_1_rate "
           "FROM server_performance WHERE server_name='"
        << server_name << "' AND timestamp BETWEEN '" << start_time << "' AND '"
        << end_time << "' ORDER BY timestamp";
  }

  if (mysql_query(conn_, sql.str().c_str()) != 0)
  {
    std::cerr << "QueryManager: trend query failed: " << mysql_error(conn_)
              << std::endl;
    return records;
  }

  MYSQL_RES *result = mysql_store_result(conn_);
  if (!result)
  {
    return records;
  }

  MYSQL_ROW row;
  while ((row = mysql_fetch_row(result)))
  {
    PerformanceRecord rec;
    int i = 0;
    rec.server_name = row[i++] ? row[i - 1] : "";
    rec.timestamp =
        row[i] ? ParseTime(row[i]) : std::chrono::system_clock::now();
    i++;
    rec.cpu_percent = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.usr_percent = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.system_percent = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.io_wait_percent = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.load_avg_1 = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.load_avg_5 = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.load_avg_15 = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.mem_used_percent = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.disk_util_percent = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.send_rate = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.rcv_rate = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.score = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.cpu_percent_rate = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.mem_used_percent_rate = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.disk_util_percent_rate = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.load_avg_1_rate = row[i] ? std::atof(row[i]) : 0;
    records.push_back(rec);
  }
  mysql_free_result(result);
#else
  (void)server_name;
  (void)time_range;
  (void)interval_seconds;
#endif

  return records;
}

std::vector<AnomalyRecord>
QueryManager::QueryAnomaly(const std::string &server_name,
                           const TimeRange &time_range,
                           const AnomalyThresholds &thresholds, int page,
                           int page_size, int *total_count)
{
  std::vector<AnomalyRecord> records;

#ifdef ENABLE_MYSQL
  std::lock_guard<std::mutex> lock(mtx_);
  if (!initialized_ || !conn_)
  {
    return records;
  }

  if (!ValidateTimeRange(time_range))
  {
    std::cerr << "QueryManager: Invalid time range" << std::endl;
    return records;
  }
  if (page < 1)
    page = 1;
  if (page_size < 1)
    page_size = 100;

  std::string start_time = FormatTime(time_range.start_time);
  std::string end_time = FormatTime(time_range.end_time);

  // 构建WHERE条件
  std::ostringstream where_clause;
  where_clause << "timestamp BETWEEN '" << start_time << "' AND '" << end_time
               << "'";
  if (!server_name.empty())
  {
    where_clause << " AND server_name='" << server_name << "'";
  }
  where_clause << " AND (cpu_percent > " << thresholds.cpu_threshold
               << " OR mem_used_percent > " << thresholds.mem_threshold
               << " OR disk_util_percent > " << thresholds.disk_threshold
               << " OR ABS(cpu_percent_rate) > "
               << thresholds.change_rate_threshold
               << " OR ABS(mem_used_percent_rate) > "
               << thresholds.change_rate_threshold << ")";

  // 获取总数
  std::ostringstream count_sql;
  count_sql << "SELECT COUNT(*) FROM server_performance WHERE "
            << where_clause.str();
  if (total_count)
  {
    *total_count = GetTotalCount(count_sql.str());
  }

  // 查询数据
  int offset = (page - 1) * page_size;
  std::ostringstream sql;
  sql << "SELECT server_name, timestamp, cpu_percent, mem_used_percent, "
         "disk_util_percent, cpu_percent_rate, mem_used_percent_rate "
         "FROM server_performance WHERE "
      << where_clause.str() << " ORDER BY timestamp DESC LIMIT " << page_size
      << " OFFSET " << offset;

  if (mysql_query(conn_, sql.str().c_str()) != 0)
  {
    std::cerr << "QueryManager: anomaly query failed: " << mysql_error(conn_)
              << std::endl;
    return records;
  }

  MYSQL_RES *result = mysql_store_result(conn_);
  if (!result)
  {
    return records;
  }

  MYSQL_ROW row;
  while ((row = mysql_fetch_row(result)))
  {
    std::string srv_name = row[0] ? row[0] : "";
    auto ts = row[1] ? ParseTime(row[1]) : std::chrono::system_clock::now();
    float cpu = row[2] ? std::atof(row[2]) : 0;
    float mem = row[3] ? std::atof(row[3]) : 0;
    float disk = row[4] ? std::atof(row[4]) : 0;
    float cpu_rate = row[5] ? std::atof(row[5]) : 0;
    float mem_rate = row[6] ? std::atof(row[6]) : 0;

    // 生成异常记录
    auto add_anomaly = [&](const std::string &type, const std::string &metric,
                           float value, float threshold)
    {
      AnomalyRecord rec;
      rec.server_name = srv_name;
      rec.timestamp = ts;
      rec.anomaly_type = type;
      rec.metric_name = metric;
      rec.value = value;
      rec.threshold = threshold;
      // 判断严重程度
      if (type == "CPU_HIGH" && value > 95)
      {
        rec.severity = "CRITICAL";
      }
      else if (type == "MEM_HIGH" && value > 95)
      {
        rec.severity = "CRITICAL";
      }
      else if (type == "DISK_HIGH" && value > 95)
      {
        rec.severity = "CRITICAL";
      }
      else if (type == "RATE_SPIKE" && std::abs(value) > 1.0)
      {
        rec.severity = "CRITICAL";
      }
      else
      {
        rec.severity = "WARNING";
      }
      records.push_back(rec);
    };

    if (cpu > thresholds.cpu_threshold)
    {
      add_anomaly("CPU_HIGH", "cpu_percent", cpu, thresholds.cpu_threshold);
    }
    if (mem > thresholds.mem_threshold)
    {
      add_anomaly("MEM_HIGH", "mem_used_percent", mem,
                  thresholds.mem_threshold);
    }
    if (disk > thresholds.disk_threshold)
    {
      add_anomaly("DISK_HIGH", "disk_util_percent", disk,
                  thresholds.disk_threshold);
    }
    if (std::abs(cpu_rate) > thresholds.change_rate_threshold)
    {
      add_anomaly("RATE_SPIKE", "cpu_percent_rate", cpu_rate,
                  thresholds.change_rate_threshold);
    }
    if (std::abs(mem_rate) > thresholds.change_rate_threshold)
    {
      add_anomaly("RATE_SPIKE", "mem_used_percent_rate", mem_rate,
                  thresholds.change_rate_threshold);
    }
  }
  mysql_free_result(result);
#else
  (void)server_name;
  (void)time_range;
  (void)thresholds;
  (void)page;
  (void)page_size;
  (void)total_count;
#endif

  return records;
}

std::vector<ServerScoreSummary> QueryManager::QueryScoreRank(SortOrder order,
                                                             int page,
                                                             int page_size,
                                                             int *total_count)
{
  std::vector<ServerScoreSummary> records;

#ifdef ENABLE_MYSQL
  std::lock_guard<std::mutex> lock(mtx_);
  if (!initialized_ || !conn_)
  {
    return records;
  }

  if (page < 1)
    page = 1;
  if (page_size < 1)
    page_size = 100;

  // 获取总数（不同服务器数量）
  std::string count_sql =
      "SELECT COUNT(DISTINCT server_name) FROM server_performance";
  if (total_count)
  {
    *total_count = GetTotalCount(count_sql);
  }

  // 查询每台服务器的最新数据并排序
  int offset = (page - 1) * page_size;
  std::string order_str = (order == SortOrder::ASC) ? "ASC" : "DESC";

  std::ostringstream sql;
  sql << "SELECT p1.server_name, p1.score, p1.timestamp, p1.cpu_percent, "
         "p1.mem_used_percent, p1.disk_util_percent, p1.load_avg_1 "
         "FROM server_performance p1 "
         "INNER JOIN ("
         "  SELECT server_name, MAX(timestamp) as max_ts "
         "  FROM server_performance GROUP BY server_name"
         ") p2 ON p1.server_name = p2.server_name AND p1.timestamp = p2.max_ts "
         "ORDER BY p1.score "
      << order_str << " LIMIT " << page_size << " OFFSET " << offset;

  if (mysql_query(conn_, sql.str().c_str()) != 0)
  {
    std::cerr << "QueryManager: score rank query failed: " << mysql_error(conn_)
              << std::endl;
    return records;
  }

  MYSQL_RES *result = mysql_store_result(conn_);
  if (!result)
  {
    return records;
  }

  auto now = std::chrono::system_clock::now();
  MYSQL_ROW row;
  while ((row = mysql_fetch_row(result)))
  {
    ServerScoreSummary rec;
    rec.server_name = row[0] ? row[0] : "";
    rec.score = row[1] ? std::atof(row[1]) : 0;
    rec.last_update = row[2] ? ParseTime(row[2]) : now;
    rec.cpu_percent = row[3] ? std::atof(row[3]) : 0;
    rec.mem_used_percent = row[4] ? std::atof(row[4]) : 0;
    rec.disk_util_percent = row[5] ? std::atof(row[5]) : 0;
    rec.load_avg_1 = row[6] ? std::atof(row[6]) : 0;

    // 判断在线状态（60秒阈值）
    auto age =
        std::chrono::duration_cast<std::chrono::seconds>(now - rec.last_update)
            .count();
    rec.status = (age > 60) ? ServerStatus::OFFLINE : ServerStatus::ONLINE;

    records.push_back(rec);
  }
  mysql_free_result(result);
#else
  (void)order;
  (void)page;
  (void)page_size;
  (void)total_count;
#endif

  return records;
}

std::vector<ServerScoreSummary>
QueryManager::QueryLatestScore(ClusterStats *stats)
{
  std::vector<ServerScoreSummary> records;

#ifdef ENABLE_MYSQL
  std::lock_guard<std::mutex> lock(mtx_);
  if (!initialized_ || !conn_)
  {
    return records;
  }

  // 查询每台服务器的最新数据
  std::string sql =
      "SELECT p1.server_name, p1.score, p1.timestamp, p1.cpu_percent, "
      "p1.mem_used_percent, p1.disk_util_percent, p1.load_avg_1 "
      "FROM server_performance p1 "
      "INNER JOIN ("
      "  SELECT server_name, MAX(timestamp) as max_ts "
      "  FROM server_performance GROUP BY server_name"
      ") p2 ON p1.server_name = p2.server_name AND p1.timestamp = p2.max_ts "
      "ORDER BY p1.score DESC";

  if (mysql_query(conn_, sql.c_str()) != 0)
  {
    std::cerr << "QueryManager: latest score query failed: "
              << mysql_error(conn_) << std::endl;
    return records;
  }

  MYSQL_RES *result = mysql_store_result(conn_);
  if (!result)
  {
    return records;
  }

  auto now = std::chrono::system_clock::now();
  float total_score = 0;
  float max_score = -1;
  float min_score = 101;
  std::string best_server, worst_server;
  int online_count = 0, offline_count = 0;

  MYSQL_ROW row;
  while ((row = mysql_fetch_row(result)))
  {
    ServerScoreSummary rec;
    rec.server_name = row[0] ? row[0] : "";
    rec.score = row[1] ? std::atof(row[1]) : 0;
    rec.last_update = row[2] ? ParseTime(row[2]) : now;
    rec.cpu_percent = row[3] ? std::atof(row[3]) : 0;
    rec.mem_used_percent = row[4] ? std::atof(row[4]) : 0;
    rec.disk_util_percent = row[5] ? std::atof(row[5]) : 0;
    rec.load_avg_1 = row[6] ? std::atof(row[6]) : 0;

    // 判断在线状态（60秒阈值）
    auto age =
        std::chrono::duration_cast<std::chrono::seconds>(now - rec.last_update)
            .count();
    rec.status = (age > 60) ? ServerStatus::OFFLINE : ServerStatus::ONLINE;

    if (rec.status == ServerStatus::ONLINE)
    {
      online_count++;
    }
    else
    {
      offline_count++;
    }

    // 统计
    total_score += rec.score;
    if (rec.score > max_score)
    {
      max_score = rec.score;
      best_server = rec.server_name;
    }
    if (rec.score < min_score)
    {
      min_score = rec.score;
      worst_server = rec.server_name;
    }

    records.push_back(rec);
  }
  mysql_free_result(result);

  // 填充集群统计
  if (stats)
  {
    stats->total_servers = static_cast<int>(records.size());
    stats->online_servers = online_count;
    stats->offline_servers = offline_count;
    stats->avg_score = records.empty() ? 0 : total_score / records.size();
    stats->max_score = max_score > 0 ? max_score : 0;
    stats->min_score = min_score < 101 ? min_score : 0;
    stats->best_server = best_server;
    stats->worst_server = worst_server;
  }
#else
  (void)stats;
#endif

  return records;
}

std::vector<NetDetailRecord>
QueryManager::QueryNetDetail(const std::string &server_name,
                             const TimeRange &time_range, int page,
                             int page_size, int *total_count)
{
  std::vector<NetDetailRecord> records;

#ifdef ENABLE_MYSQL
  std::lock_guard<std::mutex> lock(mtx_);
  if (!initialized_ || !conn_)
  {
    return records;
  }

  if (!ValidateTimeRange(time_range))
  {
    return records;
  }
  if (page < 1)
    page = 1;
  if (page_size < 1)
    page_size = 100;

  std::string start_time = FormatTime(time_range.start_time);
  std::string end_time = FormatTime(time_range.end_time);

  // 获取总数
  std::ostringstream count_sql;
  count_sql << "SELECT COUNT(*) FROM server_net_detail WHERE server_name='"
            << server_name << "' AND timestamp BETWEEN '" << start_time
            << "' AND '" << end_time << "'";
  if (total_count)
  {
    *total_count = GetTotalCount(count_sql.str());
  }

  // 查询数据
  int offset = (page - 1) * page_size;
  std::ostringstream sql;
  sql << "SELECT server_name, net_name, timestamp, err_in, err_out, "
         "drop_in, drop_out, rcv_bytes_rate, snd_bytes_rate, "
         "rcv_packets_rate, snd_packets_rate "
         "FROM server_net_detail WHERE server_name='"
      << server_name << "' AND timestamp BETWEEN '" << start_time << "' AND '"
      << end_time << "' ORDER BY timestamp DESC LIMIT " << page_size
      << " OFFSET " << offset;

  if (mysql_query(conn_, sql.str().c_str()) != 0)
  {
    std::cerr << "QueryManager: net detail query failed: " << mysql_error(conn_)
              << std::endl;
    return records;
  }

  MYSQL_RES *result = mysql_store_result(conn_);
  if (!result)
  {
    return records;
  }

  MYSQL_ROW row;
  while ((row = mysql_fetch_row(result)))
  {
    NetDetailRecord rec;
    int i = 0;
    rec.server_name = row[i++] ? row[i - 1] : "";
    rec.net_name = row[i++] ? row[i - 1] : "";
    rec.timestamp =
        row[i] ? ParseTime(row[i]) : std::chrono::system_clock::now();
    i++;
    rec.err_in = row[i] ? std::stoull(row[i]) : 0;
    i++;
    rec.err_out = row[i] ? std::stoull(row[i]) : 0;
    i++;
    rec.drop_in = row[i] ? std::stoull(row[i]) : 0;
    i++;
    rec.drop_out = row[i] ? std::stoull(row[i]) : 0;
    i++;
    rec.rcv_bytes_rate = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.snd_bytes_rate = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.rcv_packets_rate = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.snd_packets_rate = row[i] ? std::atof(row[i]) : 0;
    records.push_back(rec);
  }
  mysql_free_result(result);
#else
  (void)server_name;
  (void)time_range;
  (void)page;
  (void)page_size;
  (void)total_count;
#endif

  return records;
}

std::vector<DiskDetailRecord>
QueryManager::QueryDiskDetail(const std::string &server_name,
                              const TimeRange &time_range, int page,
                              int page_size, int *total_count)
{
  std::vector<DiskDetailRecord> records;

#ifdef ENABLE_MYSQL
  std::lock_guard<std::mutex> lock(mtx_);
  if (!initialized_ || !conn_)
  {
    return records;
  }

  if (!ValidateTimeRange(time_range))
  {
    return records;
  }
  if (page < 1)
    page = 1;
  if (page_size < 1)
    page_size = 100;

  std::string start_time = FormatTime(time_range.start_time);
  std::string end_time = FormatTime(time_range.end_time);

  // 获取总数
  std::ostringstream count_sql;
  count_sql << "SELECT COUNT(*) FROM server_disk_detail WHERE server_name='"
            << server_name << "' AND timestamp BETWEEN '" << start_time
            << "' AND '" << end_time << "'";
  if (total_count)
  {
    *total_count = GetTotalCount(count_sql.str());
  }

  // 查询数据
  int offset = (page - 1) * page_size;
  std::ostringstream sql;
  sql << "SELECT server_name, disk_name, timestamp, read_bytes_per_sec, "
         "write_bytes_per_sec, read_iops, write_iops, avg_read_latency_ms, "
         "avg_write_latency_ms, util_percent "
         "FROM server_disk_detail WHERE server_name='"
      << server_name << "' AND timestamp BETWEEN '" << start_time << "' AND '"
      << end_time << "' ORDER BY timestamp DESC LIMIT " << page_size
      << " OFFSET " << offset;

  if (mysql_query(conn_, sql.str().c_str()) != 0)
  {
    std::cerr << "QueryManager: disk detail query failed: "
              << mysql_error(conn_) << std::endl;
    return records;
  }

  MYSQL_RES *result = mysql_store_result(conn_);
  if (!result)
  {
    return records;
  }

  MYSQL_ROW row;
  while ((row = mysql_fetch_row(result)))
  {
    DiskDetailRecord rec;
    int i = 0;
    rec.server_name = row[i++] ? row[i - 1] : "";
    rec.disk_name = row[i++] ? row[i - 1] : "";
    rec.timestamp =
        row[i] ? ParseTime(row[i]) : std::chrono::system_clock::now();
    i++;
    rec.read_bytes_per_sec = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.write_bytes_per_sec = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.read_iops = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.write_iops = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.avg_read_latency_ms = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.avg_write_latency_ms = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.util_percent = row[i] ? std::atof(row[i]) : 0;
    records.push_back(rec);
  }
  mysql_free_result(result);
#else
  (void)server_name;
  (void)time_range;
  (void)page;
  (void)page_size;
  (void)total_count;
#endif

  return records;
}

std::vector<MemDetailRecord>
QueryManager::QueryMemDetail(const std::string &server_name,
                             const TimeRange &time_range, int page,
                             int page_size, int *total_count)
{
  std::vector<MemDetailRecord> records;

#ifdef ENABLE_MYSQL
  std::lock_guard<std::mutex> lock(mtx_);
  if (!initialized_ || !conn_)
  {
    return records;
  }

  if (!ValidateTimeRange(time_range))
  {
    return records;
  }
  if (page < 1)
    page = 1;
  if (page_size < 1)
    page_size = 100;

  std::string start_time = FormatTime(time_range.start_time);
  std::string end_time = FormatTime(time_range.end_time);

  // 获取总数
  std::ostringstream count_sql;
  count_sql << "SELECT COUNT(*) FROM server_mem_detail WHERE server_name='"
            << server_name << "' AND timestamp BETWEEN '" << start_time
            << "' AND '" << end_time << "'";
  if (total_count)
  {
    *total_count = GetTotalCount(count_sql.str());
  }

  // 查询数据
  int offset = (page - 1) * page_size;
  std::ostringstream sql;
  sql << "SELECT server_name, timestamp, total, free, avail, buffers, "
         "cached, active, inactive, dirty "
         "FROM server_mem_detail WHERE server_name='"
      << server_name << "' AND timestamp BETWEEN '" << start_time << "' AND '"
      << end_time << "' ORDER BY timestamp DESC LIMIT " << page_size
      << " OFFSET " << offset;

  if (mysql_query(conn_, sql.str().c_str()) != 0)
  {
    std::cerr << "QueryManager: mem detail query failed: " << mysql_error(conn_)
              << std::endl;
    return records;
  }

  MYSQL_RES *result = mysql_store_result(conn_);
  if (!result)
  {
    return records;
  }

  MYSQL_ROW row;
  while ((row = mysql_fetch_row(result)))
  {
    MemDetailRecord rec;
    int i = 0;
    rec.server_name = row[i++] ? row[i - 1] : "";
    rec.timestamp =
        row[i] ? ParseTime(row[i]) : std::chrono::system_clock::now();
    i++;
    rec.total = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.free = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.avail = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.buffers = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.cached = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.active = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.inactive = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.dirty = row[i] ? std::atof(row[i]) : 0;
    records.push_back(rec);
  }
  mysql_free_result(result);
#else
  (void)server_name;
  (void)time_range;
  (void)page;
  (void)page_size;
  (void)total_count;
#endif

  return records;
}

std::vector<SoftIrqDetailRecord>
QueryManager::QuerySoftIrqDetail(const std::string &server_name,
                                 const TimeRange &time_range, int page,
                                 int page_size, int *total_count)
{
  std::vector<SoftIrqDetailRecord> records;

#ifdef ENABLE_MYSQL
  std::lock_guard<std::mutex> lock(mtx_);
  if (!initialized_ || !conn_)
  {
    return records;
  }

  if (!ValidateTimeRange(time_range))
  {
    return records;
  }
  if (page < 1)
    page = 1;
  if (page_size < 1)
    page_size = 100;

  std::string start_time = FormatTime(time_range.start_time);
  std::string end_time = FormatTime(time_range.end_time);

  // 获取总数
  std::ostringstream count_sql;
  count_sql << "SELECT COUNT(*) FROM server_softirq_detail WHERE server_name='"
            << server_name << "' AND timestamp BETWEEN '" << start_time
            << "' AND '" << end_time << "'";
  if (total_count)
  {
    *total_count = GetTotalCount(count_sql.str());
  }

  // 查询数据
  int offset = (page - 1) * page_size;
  std::ostringstream sql;
  sql << "SELECT server_name, cpu_name, timestamp, hi, timer, net_tx, "
         "net_rx, block, sched "
         "FROM server_softirq_detail WHERE server_name='"
      << server_name << "' AND timestamp BETWEEN '" << start_time << "' AND '"
      << end_time << "' ORDER BY timestamp DESC LIMIT " << page_size
      << " OFFSET " << offset;

  if (mysql_query(conn_, sql.str().c_str()) != 0)
  {
    std::cerr << "QueryManager: softirq detail query failed: "
              << mysql_error(conn_) << std::endl;
    return records;
  }

  MYSQL_RES *result = mysql_store_result(conn_);
  if (!result)
  {
    return records;
  }

  MYSQL_ROW row;
  while ((row = mysql_fetch_row(result)))
  {
    SoftIrqDetailRecord rec;
    int i = 0;
    rec.server_name = row[i++] ? row[i - 1] : "";
    rec.cpu_name = row[i++] ? row[i - 1] : "";
    rec.timestamp =
        row[i] ? ParseTime(row[i]) : std::chrono::system_clock::now();
    i++;
    rec.hi = row[i] ? std::stoll(row[i]) : 0;
    i++;
    rec.timer = row[i] ? std::stoll(row[i]) : 0;
    i++;
    rec.net_tx = row[i] ? std::stoll(row[i]) : 0;
    i++;
    rec.net_rx = row[i] ? std::stoll(row[i]) : 0;
    i++;
    rec.block = row[i] ? std::stoll(row[i]) : 0;
    i++;
    rec.sched = row[i] ? std::stoll(row[i]) : 0;
    records.push_back(rec);
  }
  mysql_free_result(result);
#else
  (void)server_name;
  (void)time_range;
  (void)page;
  (void)page_size;
  (void)total_count;
#endif

  return records;
}

} // namespace monitor
