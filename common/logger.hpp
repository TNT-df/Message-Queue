#ifndef __M_LOG__
#define __M_LOG__
#include <iostream>
#include <ctime>
#include <string>

namespace tntmq
{

    enum LogLevel
    {
        DEBUG,
        INFO,
        WARN,
        ERROR
    };

    inline const char *LogLevelToString(LogLevel level)
    {
        switch (level)
        {
        case DEBUG:
            return "DEBUG";
        case INFO:
            return "INFO";
        case WARN:
            return "WARN";
        case ERROR:
            return "ERROR";
        default:
            return "UNKNOWN";
        }
    }

#define LOG(level, format, ...)                                                                                  \
    {                                                                                                            \
        time_t t = time(nullptr);                                                                                \
        struct tm *tm = localtime(&t);                                                                           \
        char s[32];                                                                                              \
        strftime(s, sizeof(s) - 1, "%Y-%m-%d %H:%M:%S", tm);                                                     \
        printf("[%s] [%s] [%s:%d] " format "\n", s, LogLevelToString(level), __FILE__, __LINE__, ##__VA_ARGS__); \
    }
}

#endif
