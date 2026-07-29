#include "monitor/user_monitor.h"
#include "monitor_info.pb.h"
#include "utils/read_file.h"
#include <sys/types.h>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <string>

namespace monitor
{
    static const char *kProcPasswdPath = "/etc/passwd";
    std::string UserMonitor::GetUsernameByUid(uid_t uid)
    {
        std::ifstream passwd_file(kProcPasswdPath);
        if (!passwd_file.is_open())
        {
            return "";
        }

        std::string line;
        while (std::getline(passwd_file, line))
        {
            std::istringstream iss(line);
            std::string username, password, uid_str;

            if (!std::getline(iss, username, ':') ||
                !std::getline(iss, password, ':') ||
                !std::getline(iss, uid_str, ':'))
            {
                continue; // 如果行格式不正确，跳过
            }
            try
            {
                uid_t parsed_uid = static_cast<uid_t>(std::stoul(uid_str));
                if (parsed_uid == uid)
                {
                    return username;
                }
            }
            catch (const std::exception &)
            {
                // 如果转换失败，继续处理下一行
                continue;
            }
        }
        return "";
    }

    void UserMonitor::UpdateOnce(monitor::proto::MonitorInfo *monitor_info)
    {
        if (!monitor_info)
            return;

        uid_t uid = getuid();
        std::string username = GetUsernameByUid(uid);

        if (!username.empty())
        {
            monitor_info->set_name(username);
        }
        else
        {
            monitor_info->set_name("");
        }
    }

    void UserMonitor::Stop()
    {
        // 这里可以实现优雅停止的逻辑，如果有需要的话
    }
}
