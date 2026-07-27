#pragma once

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace monitor
{
    class ReadFile
    {
    public:
        // 构造函数，接受一个文件路径作为参数，并打开该文件
        ReadFile(const std::string& file_path) : ifs_(file_path) {}

        // 析构函数，关闭文件流
        ~ReadFile() { ifs_.close(); }

        // 读取文件中的一行，并将其拆分为单词，存储在 args 向量中
        bool ReadLine(std::vector<std::string>* args);

    private:
        // 文件输入流，用于读取文件内容
        std::ifstream ifs_;
    };
}