#include <iostream>
#include <ctime>
#include <string>
// 封装日志宏，通过日志宏进行日志打印，带有系统时间和文件名和行号
enum LogLevel
{
    DEBUG,
    INFO,
    WARN,
    ERROR
};

#define LOG(level, format, ...)                                                                 \
    {                                                                                           \
        time_t t = time(nullptr);                                                               \
        struct tm *tm = localtime(&t);                                                          \
        char s[32];                                                                             \
        strftime(s, sizeof(s) - 1, "%Y-%m-%d %H:%M:%S", tm);                                    \
        printf("[%s] [%s] [%s:%d] " format "\n", s, #level, __FILE__, __LINE__, ##__VA_ARGS__); \
    }
int main()
{
    return 0;
}