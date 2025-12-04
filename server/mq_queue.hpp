#ifndef _M_QUEUE_H__
#define _M_QUEUE_H__

#include "../common/mq_logger.hpp"
#include "../common/mq_helper.hpp"
#include "../common/mq_msg.pb.h"
#include <iostream>
#include <mutex>
#include <memory>
#include <sstream>
#include <unordered_map>

namespace tntmq
{
    struct MsgQueue
    {
        using ptr = std::shared_ptr<MsgQueue>;
        std::string name;
        bool durable;
        bool exclusive;
        bool auto_delete;
        std::unordered_map<std::string, std::string> args;

        MsgQueue() {}
        MsgQueue(const std::string &qname, bool qdurable, bool qexclusive, bool qauto_delete, const std::unordered_map<std::string, std::string> &qargs)
            : name(qname), durable(qdurable), exclusive(qexclusive), auto_delete(qauto_delete), args(qargs) {}

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
                    this->args.insert(std::make_pair(key, val));
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
    class MsgQueueMapper
    {
    public:
        MsgQueueMapper(const std::string &dbfile) : _sqlite_helper(dbfile)
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
#define CREATE_SQL "CREATE TABLE IF NOT EXISTS msgQueue_table \
            (name VARCHAR(32) PRIMARY KEY,  durable INTEGER, exclusive INTEGER ,auto_delete INTEGER, args varchar(128));"
            bool ret = _sqlite_helper.exec(CREATE_SQL, nullptr, nullptr);
            if (ret == false)
            {
                LOG(LogLevel::ERROR, "创建消息队列表失败");
                abort();
            }
        }

        void removeTable()
        {
#define DROP_TABLE "drop table if exists msgQueue_table;"
            bool ret = _sqlite_helper.exec(DROP_TABLE, nullptr, nullptr);
            if (ret == false)
            {
                LOG(LogLevel::ERROR, "删除消息队列表失败");
                abort();
            }
        }

        bool insert(MsgQueue::ptr &msgQueue)
        {
            std::stringstream ss;
            // specify columns explicitly and include name
            ss << "INSERT INTO msgQueue_table (name, durable, exclusive,auto_delete, args) VALUES (";
            ss << "'" << msgQueue->name << "', ";
            ss << (msgQueue->durable ? 1 : 0) << ", ";
            ss << (msgQueue->exclusive ? 1 : 0) << ", ";
            ss << (msgQueue->auto_delete ? 1 : 0) << ", ";
            ss << "'" << msgQueue->getArgs() << "');";
            return _sqlite_helper.exec(ss.str(), nullptr, nullptr);
        }
        void remove(const std::string &name)
        {
            std::stringstream ss;
            ss << "delete from msgQueue_table where name='" << name << "';";
            bool ret = _sqlite_helper.exec(ss.str(), nullptr, nullptr);
            if (ret == false)
            {
                return;
            }
        }

        using MsgQueueMap = std::unordered_map<std::string, MsgQueue::ptr>;
        MsgQueueMap recovery()
        {
            MsgQueueMap result;
            std::stringstream ss;
            ss << "select name, durable, exclusive,auto_delete, args from msgQueue_table;";
            bool ret = _sqlite_helper.exec(ss.str(), selectCallBack, &result);
            return result;
        }

    private:
        static int selectCallBack(void *arg, int numcol, char **row, char **fields)
        {
            if (!arg || !row)
                return 1;
            MsgQueueMap *result = static_cast<MsgQueueMap *>(arg);
            if (numcol < 5)
                return 1;
            std::string name = row[0] ? row[0] : std::string();

            bool durable = row[1] ? (std::stoi(row[2]) != 0) : false;
            bool exclusive = row[2] ? (std::stoi(row[2]) != 0) : false;
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
            auto msg = std::make_shared<MsgQueue>(name, durable, exclusive, auto_del, parsedArgs);
            result->insert(std::make_pair(msg->name, msg));
            return 0;
        }

    private:
        SqliteHelper _sqlite_helper;
    };

    // 定义交换机数据内存管理类
    class MsgQueueManager
    {
    public:
        using ptr = std::shared_ptr<MsgQueueManager>;
        MsgQueueManager(const std::string &dbfile) : _mapper(dbfile)
        {

            _msg_queues = _mapper.recovery();
        }
        // 声明交换机
        bool declareMsgQueue(const std::string &name, bool durable, bool exclusive, bool auto_delete, const std::unordered_map<std::string, std::string> &eargs)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _msg_queues.find(name);
            if (it != _msg_queues.end())
            {
                return true;
            }
            auto exp = std::make_shared<MsgQueue>(name, durable, exclusive, auto_delete, eargs);
            _msg_queues.emplace(name, exp);
            if (durable == true)
            {
                return _mapper.insert(exp);
            }
            return true;
        }
        // 删除交换机
        bool deleteMsgQueue(const std::string &name)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _msg_queues.find(name);
            if (it == _msg_queues.end())
            {
                return true;
            }
            if (it->second->durable == true)
            {
                _mapper.remove(name);
            }
            return _msg_queues.erase(name);
        }
        MsgQueue::ptr selectMsgQueue(const std::string &name)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _msg_queues.find(name);
            if (it != _msg_queues.end())
            {
                return it->second;
            }
            return nullptr;
        }
        const std::unordered_map<std::string, MsgQueue::ptr> allQueues()
        {
            std::unique_lock<std::mutex> lock(_mutex);
            return _msg_queues;
        }
        bool exists(const std::string &name)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _msg_queues.find(name);
            return it != _msg_queues.end();
        }
        size_t size()
        {
            std::unique_lock<std::mutex> lock(_mutex);
            return _msg_queues.size();
        }
        // 清理所有交换机数据
        void clear()
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _mapper.removeTable();
            _msg_queues.clear();
        }

    private:
        std::mutex _mutex;
        MsgQueueMapper _mapper;
        std::unordered_map<std::string, MsgQueue::ptr> _msg_queues;
    };
}

#endif