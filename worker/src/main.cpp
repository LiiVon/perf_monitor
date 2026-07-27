#include "utils/read_file.h"
#include <iostream>
#include <vector>

int main()
{
    monitor::ReadFile rf("/proc/stat");
    std::vector<std::string> fields;
    if (rf.ReadLine(&fields))
    {
        for (const auto &s : fields)
        {
            std::cout << s << " ";
        }
        std::cout << std::endl;
    }
    return 0;
}