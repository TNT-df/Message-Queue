#ifndef _M_CONSUMER_H__
#define _M_CONSUMER_H__
#include "../common/mq_logger.hpp"
#include "../common/mq_helper.hpp"
#include "../common/mq_msg.pb.h"
#include <iostream>
#include <mutex>
#include <memory>
#include <vector>
#include <functional>

namespace tntmq
{
    using ConsumerCallBack = std::function<void(const std::string & /*tag*/, const BasicProperties *bp /*bp*/, const std::string & /*body*/)>;

    struct Consumer
    {
        using ptr = std::shared_ptr<Consumer>;
        std::string tag;   // 消费者标识
        std::string qname; // 消费者订阅的队列名称
        bool auto_ack;     // 自动确认标志
        ConsumerCallBack callback;

        Consumer()
        {
        }
        Consumer(const std::string &ctag, const std::string &queue_name, bool ack, ConsumerCallBack &cb) : tag(ctag),
                                                                                                           qname(queue_name), auto_ack(ack), callback(cb)
        {
        }
    };
    // 以队列为单元的消费者管理结构
    class QueueConsumer
    {
    public:
        using ptr = std::shared_ptr<QueueConsumer>;
        QueueConsumer(const std::string &qname) : _qname(qname), _rr_seq(0)
        {
        }
        // 队列新增消费者
        Consumer::ptr create(const std::string &ctag, const std::string &queue_name, bool ack, ConsumerCallBack &cb)
        {
            // 加锁
            std::unique_lock<std::mutex> _lock(_mutex);
            // 消费者是否重复
            for (auto const &c : _consumers)
            {
                if (c->tag == ctag)
                {
                    return Consumer::ptr();
                }
            }

            // 没有重复则新增--构造对象
            Consumer::ptr consumer = std::make_shared<Consumer>(ctag, queue_name, ack, cb);
            _consumers.emplace_back(consumer);
            // 添加管理后返回对象
            return consumer;
        }
        // 队列移除消费者
        void remove(const std::string &ctag)
        {
            // 加锁
            std::unique_lock<std::mutex> _lock(_mutex);
            // 消费者是否重复
            for (auto it = _consumers.begin(); it != _consumers.end(); ++it)
            {
                if ((*it)->tag == ctag)
                {
                    _consumers.erase(it);
                    return;
                }
            }
            return;
        }

        // 队列获取消费者
        Consumer::ptr choose()
        {
            std::unique_lock<std::mutex> _lock(_mutex);
            if (_consumers.size())
            {
                return Consumer::ptr();
            }
            // 获取当前轮转下标
            int idx = _rr_seq % _consumers.size();
            // 获取对象，进行返回
            _rr_seq++;
            return _consumers[idx];
        }
        // 是否为空
        bool empty()
        {
            std::unique_lock<std::mutex> _lock(_mutex);
            return _consumers.size();
        }
        // 判断指定消费者是否存在
        bool exists(const std::string &ctag)
        {
            std::unique_lock<std::mutex> _lock(_mutex);
            for (auto it = _consumers.begin(); it != _consumers.end(); ++it)
            {
                if ((*it)->tag == ctag)
                {
                    return true;
                }
            }
            return false;
        }
        // 清理所有消费者
        void clear()
        {
            _consumers.clear();
            _rr_seq = 0;
        }

    private:
        std::string _qname;
        std::mutex _mutex;
        uint64_t _rr_seq; // 轮转序号
        std::vector<Consumer::ptr> _consumers;
    };

    class ConsumerManager
    {
    private:
        std::unordered_map<std::string, QueueConsumer::ptr> _qconsumers;
        std::mutex _mutex;

    public:
        using ptr = std::shared_ptr<ConsumerManager>;
        ConsumerManager()
        {
        }
        void initQueueConsumer(const std::string &qname)
        {
            std::unique_lock<std::mutex> _lock(_mutex);
            if (_qconsumers.find(qname) == _qconsumers.end())
            {
                _qconsumers.insert(std::make_pair(qname, std::make_shared<QueueConsumer>(qname)));
            }
        }
        void destroyQueueConsumer(const std::string &qname)
        {
            std::unique_lock<std::mutex> _lock(_mutex);
            _qconsumers.erase(qname);
        }
        Consumer::ptr create(const std::string &ctag, const std::string &queue_name, bool ack, ConsumerCallBack &cb)
        {
            std::unique_lock<std::mutex> _lock(_mutex);
            auto &qconsumer = _qconsumers[queue_name];
            return qconsumer->create(ctag, queue_name, ack, cb);
        }
        void remove(const std::string &ctag, const std::string &queue_name)
        {
            std::unique_lock<std::mutex> _lock(_mutex);
            auto &qconsumer = _qconsumers[queue_name];
            qconsumer->remove(ctag);
        }
        Consumer::ptr choose(const std::string &queue_name)
        {
            std::unique_lock<std::mutex> _lock(_mutex);
            auto &qconsumer = _qconsumers[queue_name];
            return qconsumer->choose();
        }
        bool empty(const std::string &queue_name)
        {
            std::unique_lock<std::mutex> _lock(_mutex);

            return _qconsumers[queue_name]->empty();
        }
        bool exists(const std::string &ctag, const std::string &queue_name)
        {
            std::unique_lock<std::mutex> _lock(_mutex);
            auto &qconsumer = _qconsumers[queue_name];
            if (qconsumer != nullptr)
            {
                return qconsumer->exists(ctag);
            }
            return false;
        }
        void clear()
        {
            std::unique_lock<std::mutex> _lock(_mutex);
            _qconsumers.clear();
        }
    };

};
#endif