
/**
 * 封装一个SqliteHelper类，提供简单的sqlite数据库操作接口，完成数据的基础增删查改
 * 1、创建/打开数据库文件
 * 2、针对打开的数据库执行操作
 *  表的操作、数据库操作
 * 3、关闭数据库
 */
#ifndef __M_HELPER__
#define __M_HELPER__
#include "logger.hpp"
#include <iostream>
#include <sqlite3.h>
#include <vector>
#include <string>
#include <random>
#include <iomanip>
#include <atomic>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fstream>
#include <cstdio>
#include <sstream>
#include <cstring>
#include <cerrno>
namespace tntmq
{
    class SqliteHelper
    {
    private:
        std::string _dbfile;
        sqlite3 *_handler;

    public:
        typedef int (*SqliteCallBack)(void *, int, char **, char **);
        SqliteHelper(const std::string &dbfile) : _dbfile(dbfile), _handler(nullptr)
        {
        }
        bool open(int safe_level = SQLITE_OPEN_FULLMUTEX)
        {
            int ret = sqlite3_open_v2(_dbfile.c_str(), &_handler, safe_level | SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE, nullptr);
            if (ret != SQLITE_OK)
            {
                LOG(LogLevel::ERROR, "打开/创建sqllite数据库失败:%s", sqlite3_errmsg(_handler));
                return false;
            }
            return true;
        }
        bool exec(const std::string sql, SqliteCallBack cb, void *args)
        {
            int ret = sqlite3_exec(_handler, sql.c_str(), cb, args, nullptr);
            if (ret != SQLITE_OK)
            {
                LOG(LogLevel::ERROR, "%s \n执行语句失败:%s", sql.c_str(), sqlite3_errmsg(_handler));
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

    class StrHelper
    {
    public:
        static size_t split(const std::string &str, const std::string &sep, std::vector<std::string> &out)
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
    };

    class UUIDHelper
    {
    public:
        static std::string generateUUID()
        {
            std::random_device rd;

            std::mt19937_64 gen(rd()); // 梅森旋转算法，生成伪随机数

            // 限定数据区间
            std::uniform_int_distribution<int> distribution(0, 255);

            // 将生成的数字转为16进制数字字符
            std::stringstream ss;
            for (int i = 0; i < 8; i++)
            {
                ss << std::setw(2) << std::setfill('0') << std::hex << distribution(gen);
                if (i == 3 || i == 5 || i == 7)
                {
                    ss << "-";
                }
                // 定义原子类型整数初始化为1
            }
            static std::atomic<size_t> seq(1);
            size_t s = seq.fetch_add(1);
            for (int i = 7; i >= 0; i--)
            {
                ss << std::setw(2) << std::setfill('0') << std::hex << ((s >> i * 8) & 0xFF);
                if (i == 6)
                    ss << "-";
            }
            return ss.str();
        }
    };

    class FileHelper
    {
    public:
        FileHelper(const std::string &filename) : _filename(filename)
        {
        }
        bool exists()
        {
            struct stat st;
            return stat(_filename.c_str(), &st) == 0;
        }
        size_t size()
        {
            if (exists())
            {
                struct stat st;
                stat(_filename.c_str(), &st);
                return st.st_size;
            }
            else
                return 0;
        }

        bool read(std::string &body)
        {
            //获取文件大小，根据文件大小调整body大小
            size_t size = this->size();
            body.resize(size);
            return read(&body[0], 0, size);
        }
        bool read(char *body, size_t offset, size_t len)
        {
            // 打开文件
            std::ifstream is(_filename, std::ios::binary);

            if (is.is_open() == false)
            {
                LOG(LogLevel::ERROR, "打开文件%s失败", _filename.c_str());
                return false;
            }
            // 调转文件读写位置
            is.seekg(offset, std::ios::beg);
            // 3.读取文件内容
            is.read(body, len);
            if (is.good() == false)
            {
                LOG(LogLevel::ERROR, "读取文件%s失败", _filename.c_str());
                is.close();
                return false;
            }
            // 4.关闭文件
            is.close();
            return true;
        }

        bool write(const std::string &body)
        {
            return write(body.c_str(), 0, body.size());
        }
        bool write(const char *body, size_t offset, size_t len)
        {
            // 打开文件
            std::fstream fs(_filename, std::ios::binary | std::ios::in | std::ios::out);
            if (fs.is_open() == false)
            {
                LOG(LogLevel::ERROR, "打开文件%s失败", _filename.c_str());
                return false;
            }
            // 调转文件读写位置(必须具备文件读权限)
            fs.seekp(offset, std::ios::beg);
            fs.write(body, len);
            if (fs.good() == false)
            {
                LOG(LogLevel::ERROR, "写入文件%s失败", _filename.c_str());
                fs.close();
                return false;
            }
            fs.close();
            return true;
        }
        bool rename(const std::string &newname)
        {
            return (::rename(_filename.c_str(), newname.c_str())) == 0;
        }
        static bool createFile(const std::string &filename)
        {
            std::fstream ofs(filename, std::ios::binary | std::ios::out);
            if (ofs.is_open() == false)
            {
                LOG(LogLevel::ERROR, "创建文件%s失败", filename.c_str());
                return false;
            }
            else
            {
                ofs.close();
                return true;
            }
        }

       static bool removeFile(const std::string &filename)
        {
            if (::remove(filename.c_str()) == 0)
                return true;
            LOG(LogLevel::ERROR, "removeFile %s failed: %s", filename.c_str(), strerror(errno));
            return false;
        }

        static bool createDirectory(const std::string &path)
        {
            // 多级路径创建需要从第一个父级开始创建
            size_t pos = 0, idx = 0;
            while (idx < path.size())
            {
                pos = path.find('/', idx);
                if (pos == std::string ::npos)
                {
                    return (mkdir(path.c_str(), 0775)) == 0;
                }
                else
                {
                    std::string subpath = path.substr(0, pos);
                    int ret = mkdir(subpath.c_str(), 0775);
                    if (ret != 0 && errno != EEXIST)
                    {
                        LOG(LogLevel::ERROR, "创建目录%s失败:%s", subpath.c_str(), strerror(errno));
                        return false;
                    }
                    idx = pos + 1;
                }
            }
            return true;
        }
        static bool removeDirectory(const std::string &path)
        {
            std::string cmd = "rm -rf" + path;
            return system(cmd.c_str()) != -1;
        }

        static std::string parentDirectory(const std::string &filename)
        {
            size_t pos = filename.rfind('/');
            if (pos != std::string::npos)
            {
                return filename.substr(0, pos);
            }
            else
            {
                return "./";
            }
        }

    private:
        std::string _filename;
    };
}
#endif