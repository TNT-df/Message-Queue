
/**
 * 封装一个SqliteHelper类，提供简单的sqlite数据库操作接口，完成数据的基础增删查改
 * 1、创建/打开数据库文件
 * 2、针对打开的数据库执行操作
 *  表的操作、数据库操作
 * 3、关闭数据库
 */

#include <iostream>
#include <sqlite3.h>
#include <vector>
#include <string>
class SqliteHelper
{
private:
    std::string _dbfile;
    sqlite3 *_handler;

public:
    typedef int (*SqliteCallBack)(void *, int, char **, char **);
    SqliteHelper(const std::string &dbfile) : _dbfile(dbfile),_handler(nullptr)
    {
    }
    bool open(int safe_level = SQLITE_OPEN_FULLMUTEX)
    {
        int ret = sqlite3_open_v2(_dbfile.c_str(), &_handler, safe_level | SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE, nullptr);
        if (ret != SQLITE_OK)
        {
            std::cout << "打开/创建sqllite数据库失败" << std::endl;
            std::cerr << sqlite3_errmsg(_handler) << std::endl;
            return false;
        }
        return true;
    }
    bool exec(const std::string sql, SqliteCallBack cb, void *args)
    {
        int ret = sqlite3_exec(_handler, sql.c_str(), cb, args, nullptr);
        if (ret != SQLITE_OK)
        {
            std::cout << "执行语句失败" << std::endl;
            std::cout << sqlite3_errmsg(_handler) << std::endl;
            return false;
        }
        return true;
    }
    void close()
    {
        int ret = sqlite3_close_v2(_handler);
    }
    ~SqliteHelper()
    {
    }
};
