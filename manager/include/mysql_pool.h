#pragma once

// MySQL 连接池：预创建多条连接，借出/归还复用，避免每次写库都重新建连。

#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>
#include <string>

#ifdef ENABLE_MYSQL
#include <mysql/mysql.h>
#endif

namespace monitor
{

    class MysqlPool
    {
    public:
        MysqlPool() = default;

        ~MysqlPool()
        {
#ifdef ENABLE_MYSQL
            std::lock_guard<std::mutex> lock(mtx_);
            while (!pool_.empty())
            {
                mysql_close(pool_.front());
                pool_.pop();
            }
#endif
        }

        // 初始化连接池：预创建 pool_size 条连接
        bool Init(const std::string &host, const std::string &user,
                  const std::string &password, const std::string &database,
                  size_t pool_size = 4)
        {
#ifdef ENABLE_MYSQL
            std::lock_guard<std::mutex> lock(mtx_);
            host_ = host;
            user_ = user;
            password_ = password;
            database_ = database;

            for (size_t i = 0; i < pool_size; ++i)
            {
                MYSQL *conn = CreateConnection();
                if (!conn)
                {
                    std::cerr << "MysqlPool: failed to create connection "
                              << i << std::endl;
                    // 清理已创建的连接
                    while (!pool_.empty())
                    {
                        mysql_close(pool_.front());
                        pool_.pop();
                    }
                    return false;
                }
                pool_.push(conn);
            }
            std::cout << "MysqlPool: created " << pool_size << " connections"
                      << std::endl;
            return true;
#else
            (void)host;
            (void)user;
            (void)password;
            (void)database;
            (void)pool_size;
            return false;
#endif
        }

        // 从池中借出一条连接（阻塞等待）
        MYSQL *Acquire()
        {
#ifdef ENABLE_MYSQL
            std::unique_lock<std::mutex> lock(mtx_);
            cv_.wait(lock, [this] { return !pool_.empty(); });

            MYSQL *conn = pool_.front();
            pool_.pop();

            // 检测连接是否存活，断了就重连
            if (mysql_ping(conn) != 0)
            {
                std::cerr << "MysqlPool: connection lost, reconnecting..."
                          << std::endl;
                mysql_close(conn);
                conn = CreateConnection();
                if (!conn)
                {
                    // 重连失败，创建一个新连接重试
                    conn = CreateConnection();
                }
            }
            return conn;
#else
            return nullptr;
#endif
        }

        // 归还连接到池中
        void Release(MYSQL *conn)
        {
#ifdef ENABLE_MYSQL
            if (!conn)
                return;
            std::lock_guard<std::mutex> lock(mtx_);
            pool_.push(conn);
            cv_.notify_one();
#endif
        }

    private:
#ifdef ENABLE_MYSQL
        MYSQL *CreateConnection()
        {
            MYSQL *conn = mysql_init(nullptr);
            if (!conn)
                return nullptr;

            if (!mysql_real_connect(conn, host_.c_str(), user_.c_str(),
                                    password_.c_str(), database_.c_str(),
                                    0, nullptr, 0))
            {
                std::cerr << "MysqlPool: mysql_real_connect failed: "
                          << mysql_error(conn) << std::endl;
                mysql_close(conn);
                return nullptr;
            }

            mysql_set_character_set(conn, "utf8mb4");
            return conn;
        }

        std::queue<MYSQL *> pool_;
        std::string host_, user_, password_, database_;
#endif
        std::mutex mtx_;
        std::condition_variable cv_;
    };

} // namespace monitor
