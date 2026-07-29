#include "utils/read_file.h"

namespace monitor
{
    bool ReadFile::ReadLine(std::vector<std::string>* args)
    {
        std::string line;
        std::getline(ifs_, line);

        // 结束或空行 
        if(ifs_.eof() || line.empty())
        {
            return false;
        }

        
        // 用字符串流拆分
        std::istringstream line_ss(line);
        // 循环提取每个单词
        while(!line_ss.eof())
        {
            std::string word;
            line_ss >> word;
            args->push_back(word);
        }
        return true;
    }
}