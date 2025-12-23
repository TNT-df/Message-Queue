#ifndef _M_EXCHANGE_H__
#define _M_EXCHANGE_H__
#include "../common/mq_logger.hpp"
#include "../common/mq_helper.hpp"
#include "../common/mq_msg.pb.h"
#include <google/protobuf/map.h>
#include <iostream>
#include <mutex>
#include <memory>
#include <sstream>
#include <unordered_map>
namespace tntmq
{
    // 定义交换机类
    struct Exchange
    {
        using ptr = std::shared_ptr<Exchange>;
        // 名称
        std::string name;
        // 交换级类型
        ExchangeType type;
        // 是否持久化
        bool durable;
        // 是否自动删除标志
        bool auto_delete;
        // 其他参数
        google::protobuf::Map<std::string, std::string> args;

        Exchange()
        {
        }
        Exchange(const std::string &ename, ExchangeType etype, bool edurable, bool auto_delete_flag, const google::protobuf::Map<std::string, std::string> &eargs)
            : name(ename), type(etype), durable(edurable), auto_delete(auto_delete_flag), args(eargs) {}
        void setArgs(const std::string &str)
        // 存储键值对，在存储数据库的时候，会组织一个格式字符串进行存储 key=val&key=val 将内容存储到成员中
        {
            std::vector<std::string> keyAndVal;
            tntmq::StrHelper::split(str, "&", keyAndVal);
            // overwrite existing args
            this->args.clear();
            for (const auto &kv : keyAndVal)
            {
                size_t pos = kv.find('=');
                if (pos != std::string::npos)
                {
                    std::string key = kv.substr(0, pos);
                    std::string val = kv.substr(pos + 1);
                    args[key] = val;
                }
            }
        }
        std::string getArgs() // 将args中的内容序列化为字符串
        {
            // return deterministic string: sort keys
            std::vector<std::string> keys;
            keys.reserve(this->args.size());
            for (const auto &kv : this->args)
                keys.push_back(kv.first);
            std::sort(keys.begin(), keys.end());
            std::string res;
            for (const auto &k : keys)
            {
                if (!res.empty())
                    res += "&";
                res += k + "=" + this->args.at(k);
            }
            return res;
        }
    };
    // 定义交换机数据持久化管理类 --  存储在sqlit数据库中
    class ExchangeMapper
    {
    public:
        ExchangeMapper(const std::string &dbfile) : _sqlite_helper(dbfile)
        {
            std::string path = tntmq::FileHelper::parentDirectory(dbfile);
            if (!tntmq::FileHelper::createDirectory(path))
            {
                LOG(LogLevel::ERROR, "创建数据库目录%s失败", path.c_str());
            }
            assert(_sqlite_helper.open());
            createTable();
        }

        void createTable()
        {
#define CREATE_SQL "CREATE TABLE IF NOT EXISTS exchange_table \
            (name VARCHAR(32) PRIMARY KEY, type INTEGER, durable INTEGER, auto_delete INTEGER, args varchar(128));"
            bool ret = _sqlite_helper.exec(CREATE_SQL, nullptr, nullptr);
            if (ret == false)
            {
                LOG(LogLevel::ERROR, "创建交换机表失败");
                abort();
            }
        }

        void removeTable()
        {
#define DROP_TABLE "drop table if exists exchange_table;"
            bool ret = _sqlite_helper.exec(DROP_TABLE, nullptr, nullptr);
            if (ret == false)
            {
                LOG(LogLevel::ERROR, "删除交换机表失败");
                abort();
            }
        }

        void insert(Exchange::ptr &exchange)
        {
            std::stringstream ss;
            // specify columns explicitly and include name
            ss << "INSERT INTO exchange_table (name, type, durable, auto_delete, args) VALUES (";
            ss << "'" << exchange->name << "', ";
            ss << exchange->type << ", ";
            ss << (exchange->durable ? 1 : 0) << ", ";
            ss << (exchange->auto_delete ? 1 : 0) << ", ";
            ss << "'" << exchange->getArgs() << "');";
            bool ret = _sqlite_helper.exec(ss.str(), nullptr, nullptr);
            if (ret == false)
            {
                return;
            }
        }
        void remove(const std::string &name)
        {
            std::stringstream ss;
            ss << "delete from exchange_table where name='" << name << "';";
            bool ret = _sqlite_helper.exec(ss.str(), nullptr, nullptr);
            if (ret == false)
            {
                return;
            }
        }

        using ExchangeMap = std::unordered_map<std::string, Exchange::ptr>;
        ExchangeMap recovery()
        {
            std::unordered_map<std::string, Exchange::ptr> result;
            std::stringstream ss;
            ss << "select name, type, durable, auto_delete, args from exchange_table;";
            bool ret = _sqlite_helper.exec(ss.str(), selectCallBack, &result);
            return result;
        }

    private:
        static int selectCallBack(void *arg, int numcol, char **row, char **fields)
        {
            if (!arg || !row)
                return 1;
            ExchangeMap *result = static_cast<ExchangeMap *>(arg);
            if (numcol < 5)
                return 1;
            std::string name = row[0] ? row[0] : std::string();
            ExchangeType type = ExchangeType::DIRECT;
            try
            {
                if (row[1])
                    type = static_cast<ExchangeType>(std::stoi(row[1]));
            }
            catch (...)
            {
            }
            bool durable = row[2] ? (std::stoi(row[2]) != 0) : false;
            bool auto_del = row[3] ? (std::stoi(row[3]) != 0) : false;
            std::unordered_map<std::string, std::string> parsedArgs;
            if (row[4])
            {
                std::vector<std::string> kvs;
                StrHelper::split(row[4], "&", kvs);
                for (const auto &kv : kvs)
                {
                    size_t pos = kv.find('=');
                    if (pos != std::string::npos)
                        parsedArgs.emplace(kv.substr(0, pos), kv.substr(pos + 1));
                }
            }
            auto exp = std::make_shared<Exchange>(name, type, durable, auto_del, parsedArgs);
            result->insert(std::make_pair(exp->name, exp));
            return 0;
        }

    private:
        SqliteHelper _sqlite_helper;
    };

    // 定义交换机数据内存管理类
    class ExchangeManager
    {
    public:
        using ptr = std::shared_ptr<ExchangeManager>;
        ExchangeManager(const std::string &dbfile) : _mapper(dbfile)
        {

            _exchanges = _mapper.recovery();
        }
        // 声明交换机
        void declareExchange(const std::string &name, ExchangeType type, bool durable, bool auto_delete, const google::protobuf::Map<std::string, std::string> &eargs)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _exchanges.find(name);
            if (it != _exchanges.end())
            {
                // 交换机已经存在，直接返回不需要新增
                return;
            }
            auto exp = std::make_shared<Exchange>(name, type, durable, auto_delete, eargs);
            // always insert the newly created exchange into in-memory map
            _exchanges.emplace(name, exp);
            if (durable == true)
            {
                _mapper.insert(exp);
            }
        }
        // 删除交换机
        void deleteExchange(const std::string &name)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _exchanges.find(name);
            if (it == _exchanges.end())
            {
                // 交换机不存在，直接返回
                return;
            }
            if (it->second->durable == true)
            {
                _mapper.remove(name);
            }
            _exchanges.erase(name);
        }
        // 获取指定交换机对象
        Exchange::ptr selectExchange(const std::string &name)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _exchanges.find(name);
            if (it != _exchanges.end())
            {
                return it->second;
            }
            return nullptr;
        }
        // 判断交换机是否存在
        bool exists(const std::string &name)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _exchanges.find(name);
            return it != _exchanges.end();
        }
        size_t size()
        {
            std::unique_lock<std::mutex> lock(_mutex);
            return _exchanges.size();
        }
        // 清理所有交换机数据
        void clear()
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _mapper.removeTable();
            _exchanges.clear();
        }

    private:
        std::mutex _mutex;
        ExchangeMapper _mapper;
        std::unordered_map<std::string, Exchange::ptr> _exchanges;
    };
}

#endif