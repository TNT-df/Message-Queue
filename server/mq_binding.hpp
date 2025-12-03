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
    struct Binding
    {
        using ptr = std::shared_ptr<Binding>;
        std::string exchange_name;
        std::string queue_name;
        std::string binding_key;
        Binding() {}
        Binding(const std::string &ex_name, const std::string &q_name, const std::string &b_key)
            : exchange_name(ex_name), queue_name(q_name), binding_key(b_key) {}
    };
    // 队列与绑定信息一一对应，（某个交换机去绑定队列，一个交换机会有多个队列的绑定信息），先定义队列名与绑定信息映射关系方便通过队列名查找绑定信息
    using MsgQueueBindingMap = std::unordered_map<std::string, Binding::ptr>;
    // 交换机名称与队列绑定信息映射关系
    using BindingMap = std::unordered_map<std::string, MsgQueueBindingMap>;

    class BindingMapper
    {
    public:
        BindingMapper(const std::string &dbfile) : _sqlite_helper(dbfile)
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
#define CREATE_SQL "CREATE TABLE IF NOT EXISTS binding_table \
            (exchange_name VARCHAR(32), msgqueue_name VARCHAR(32), binding_key VARCHAR(128));"
            bool ret = _sqlite_helper.exec(CREATE_SQL, nullptr, nullptr);
            if (ret == false)
            {
                LOG(LogLevel::ERROR, "创建绑定表失败");
                abort();
            }
        }
        void removeTable()
        {
#define DROP_TABLE "drop table if exists binding_table;"
            bool ret = _sqlite_helper.exec(DROP_TABLE, nullptr, nullptr);
            if (ret == false)
            {
                LOG(LogLevel::ERROR, "删除绑定表失败");
                abort();
            }
        }

        bool insert(Binding::ptr &binding)
        {
            std::stringstream ss;
            ss << "INSERT INTO binding_table VALUES (";
            ss << "'" << binding->exchange_name << "', ";
            ss << "'" << binding->queue_name << "', ";
            ss << "'" << binding->binding_key << "');";
            return _sqlite_helper.exec(ss.str(), nullptr, nullptr);
        }
        void remove(const std::string &exchange_name, const std::string &queue_name)
        {
            std::stringstream ss;
            ss << "delete from binding_table where exchange_name='" << exchange_name << "' and msgqueue_name='" << queue_name << "';";
            _sqlite_helper.exec(ss.str(), nullptr, nullptr);
        }
        void removeExchangeBindings(const std::string &exchange_name)
        {
            std::stringstream ss;
            ss << "delete from binding_table where exchange_name='" << exchange_name << "';";
            bool ret = _sqlite_helper.exec(ss.str(), nullptr, nullptr);
            if (ret == false)
            {
                return;
            }
        }
        void removeQueueBindings(const std::string &queue_name)
        {
            std::stringstream ss;
            ss << "delete from binding_table where queue_name='" << queue_name << "';";
            bool ret = _sqlite_helper.exec(ss.str(), nullptr, nullptr);
            if (ret == false)
            {
                return;
            }
        }
        BindingMap recovery()
        {
            BindingMap result;
            std::stringstream ss;
            ss << "select exchange_name, msgqueue_name, binding_key from binding_table;";
            bool ret = _sqlite_helper.exec(ss.str(), selectCallBack, &result);
            return result;
        }

    private:
        static int selectCallBack(void *arg, int numcol, char **row, char **fields)
        {
            BindingMap *result = static_cast<BindingMap *>(arg);
            Binding::ptr binding = std::make_shared<Binding>(row[0], row[1], row[2]);
            // 为了防止  交换机相关的绑定信息已经存在，因此不能直接创建队列映射，进行添加，会覆盖历史数据
            // 先获取交换机对应的映射对象，往里面添加数据，若没有交换机对应的映射对象，则会使用引用自动创建一个空的映射对象
            MsgQueueBindingMap &mq_map = (*result)[binding->exchange_name];
            mq_map.insert(std::make_pair(binding->queue_name, binding));
            return 0;
        }

    private:
        SqliteHelper _sqlite_helper;
    };

    class BindingManager
    {
    public:
        using ptr = std::shared_ptr<BindingManager>;
        BindingManager(const std::string &dbfile) : _mapper(dbfile)
        {
            _bindings = _mapper.recovery();
        }
        bool bind(const std::string &ex_name, const std::string &q_name, const std::string &b_key, bool durable)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _bindings.find(ex_name);
            if (it != _bindings.end() && it->second.find(q_name) != it->second.end())
            {
                // 绑定关系已经存在，直接返回
                return true;
            }
            auto binding = std::make_shared<Binding>(ex_name, q_name, b_key);
            if (durable == true)
            {
                bool ret = _mapper.insert(binding);
                if (ret == false)
                {
                    LOG(LogLevel::ERROR, "持久化绑定关系失败 %s - %s", ex_name.c_str(), q_name.c_str());
                    return false;
                }
            }
            auto &qbmap = _bindings[ex_name];
            qbmap.insert(std::make_pair(q_name, binding));
            return true;
        }

        void unbind(const std::string &ex_name, const std::string &q_name)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it_ex = _bindings.find(ex_name);
            if (it_ex != _bindings.end())
            {
                auto &mq_map = it_ex->second;
                auto it_q = mq_map.find(q_name);
                if (it_q != mq_map.end())
                {
                    mq_map.erase(it_q);
                    _mapper.remove(ex_name, q_name);
                    if (mq_map.empty())
                    {
                        _bindings.erase(it_ex);
                    }
                }
            }
        }

        void removeExchangeBindings(const std::string &ex_name)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it_ex = _bindings.find(ex_name);
            if (it_ex != _bindings.end())
            {
                _bindings.erase(it_ex);
                _mapper.removeExchangeBindings(ex_name);
            }
        }

        void removeQueueBindings(const std::string &q_name)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _mapper.removeQueueBindings(q_name);
            for (auto it_ex = _bindings.begin(); it_ex != _bindings.end();)
            {
                auto &mq_map = it_ex->second;
                auto it_q = mq_map.find(q_name);
                if (it_q != mq_map.end())
                {
                    mq_map.erase(it_q);
                    // 如果该交换机下没有绑定的队列了，则删除该交换机的映射
                    if (mq_map.empty())
                    {
                        it_ex = _bindings.erase(it_ex);
                        continue;
                    }
                }
                ++it_ex;
            }
        }

        MsgQueueBindingMap getExchangeBindings(const std::string &ex_name)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it_ex = _bindings.find(ex_name);
            if (it_ex != _bindings.end())
            {
                return it_ex->second;
            }
            return MsgQueueBindingMap{};
        }

        Binding::ptr getBinding(const std::string &ex_name, const std::string &q_name)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it_ex = _bindings.find(ex_name);
            if (it_ex != _bindings.end())
            {
                auto &mq_map = it_ex->second;
                auto it_q = mq_map.find(q_name);
                if (it_q != mq_map.end())
                {
                    return it_q->second;
                }
            }
            return Binding::ptr();
        }

        bool exists(const std::string &ex_name, const std::string &q_name)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it_ex = _bindings.find(ex_name);
            if (it_ex != _bindings.end())
            {
                auto &mq_map = it_ex->second;
                auto it_q = mq_map.find(q_name);
                if (it_q != mq_map.end())
                {
                    return true;
                }
            }
        }

        size_t size()
        {
            size_t total = 0;
            std::unique_lock<std::mutex> lock(_mutex);
            for (const auto &pair : _bindings)
            {
                total += pair.second.size();
            }
            return total;
        }

        void clear()
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _mapper.removeTable();
            _bindings.clear();
        }

    private:
        BindingMapper _mapper;
        BindingMap _bindings;
        std::mutex _mutex;
    };
};