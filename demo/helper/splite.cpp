#include <iostream>
#include <string>
#include <vector>

size_t split(const std::string &str, const std::string &sep, std::vector<std::string> &out)
{
    size_t start = 0;
    size_t pos = str.find(sep, start);

    while (pos != std::string::npos)
    {
        // 只添加非空子串
        if (pos > start) 
        {
            out.push_back(str.substr(start, pos - start));
        }
        start = pos + sep.size();
        pos = str.find(sep, start);
    }

    // 处理最后一段
    if (start < str.length())
    {
        out.push_back(str.substr(start));
    }

    return out.size();
}
int main()
{
    std::string testStr = "news....music.#.pop";
    std::vector<std::string> result;
    int n = split(testStr, ".", result);
    for (const auto &s : result)
    {
        std::cout << s << std::endl;
    }   

    return 0;
}
