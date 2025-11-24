#include "../common/mq_logger.hpp"
#include "../common/mq_helper.hpp"
#include "../common/mq_msg.pb.h"
#include <iostream>
#include <unordered_map>
namespace tntmq
{
    // 定义交换机类
    struct Exchange
    {
        using ptr = std::shared_ptr<Exchange>();
        // 名称
        std::string name;
        // 交换级类型
        ExchangeType type;
        // 是否持久化
        bool durable;
        // 是否自动删除标志
        bool auto_delete;
        // 其他参数
        std::unordered_map<std::string, std::string> args;
        Exchange(const std::string ename, ExchangeType etype, bool edurable, bool auto_delete, std::unordered_map<std::string, std::string> &eargs)
            : name(ename), type(etype), durable(edurable), auto_delete(auto_delete), args(eargs) {}
        void setArgs(const std::string &args)
        // 存储键值对，在存储数据库的时候，会组织一个格式字符串进行存储 key=val&key=val 将内容存储到成员中
        {
            std::vector<std::string> keyAndVal;
            tntmq::StrHelper::split(args, "&", keyAndVal);
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
            std::string res;
            for (const auto &kv : this->args)
            {
                if (!res.empty())
                    res += "&";
                res += kv.first + "=" + kv.second;
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
                LOG(LogLevel::ERROR, "删除交换机表是啊失败");
                abort();
            }
        }

        void insert(Exchange::ptr &exchange)
        {
            std::stringstream ss;
            ss << "INSERT INTO exchange_table VALUES (";
            ss << "'" << exchange->type << "', ";
            ss << "'" << exchange->durable << "', ";
            ss << "'" << exchange->auto_delete << "', ";
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

        using ExchangeMap = std::unordered_map<const std::string, Exchange::ptr>;
        ExchangeMap recovery()
        {

            std::unordered_map<std::string, Exchange::ptr> result;
            std::stringstream ss;
            ss << "select name, type, durable, auto_delete, args from exchange_table;";
            bool ret = _sqlite_helper.exec(ss.str(), selectCallBack);
        }

    private:
        static int selectCallBack(void *arg, int numcol, char **row, char **fields)
        {
            ExchangeMap *result = (Exchange *)arg;
            auto exp = std::make_shared<Exchange>();
            exp->name = row[0];
            exp->type = (tntmq::ExchangeType)std::stoi(row[1]);
            exp->durable = (bool)std::stoi(row[2]);
            exp->auto_delete = (bool)std::stoi(row[3]);
            if (row[4])
                exp->setArgs(row[4]);
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
        ExchangeManager(const std::string &dbfile);
        // 生命交换机
        void declareExchange(const std::string &name, ExchangeType type, bool durable, bool auto_delete, std::unordered_map<std::string, std::string> &eargs);
        // 删除交换机
        void deleteExchange(const std::string &name);
        // 判断交换机是否存在
        bool exists(const std::string &name);
        // 清理所有交换机数据
        void clear();

    private:
        std::mutex _mutex;
        ExchangeMapper _mapper;
        std::unordered_map<std::string, Exchange::ptr> _exchanges;
    };
}