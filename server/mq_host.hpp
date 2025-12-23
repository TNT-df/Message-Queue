#include "mq_exchange.hpp"
#include "mq_queue.hpp"
#include "mq_binding.hpp"
#include "mq_message.hpp"

namespace tntmq
{
    class VirtualHost
    {
    public:
        using ptr = std::shared_ptr<VirtualHost>;
        VirtualHost(const std::string &hname, const std::string &basedir, const std::string &dbfile) : _host_name(hname),
                                                                                                       _emp(std::make_shared<ExchangeManager>(dbfile)),
                                                                                                       _mqm(std::make_shared<MsgQueueManager>(dbfile)),
                                                                                                       _bmp(std::make_shared<BindingManager>(dbfile)),
                                                                                                       _mmp(std::make_shared<MessageManager>(basedir))
        {
            // 获取所有队列信息，通过队列名称恢复历史消息
            MsgQueueMapper::MsgQueueMap qm = _mqm->allQueues();
            for (auto &q : qm)
            {
                _mmp->initQueueMessage(q.first);
            }
        }
        bool declareExchange(const std::string &name,
                             ExchangeType type, bool durable, bool auto_delete,
                             const google::protobuf::Map<std::string, std::string> &args)
        {
            _emp->declareExchange(name, type, durable, auto_delete, args);
        }

        bool existsExchange(const std::string &name)
        {
            return _emp->exists(name);
        }

        Exchange::ptr selectExchange(const std::string &ename)
        {
            return _emp->selectExchange(ename);
        }
        
        void deleteExchange(const std::string &name)
        {
            // 删除交换机需要将相关的绑定信息也删掉
            _bmp->removeExchangeBindings(name);
            _emp->deleteExchange(name);
        }

        bool declareQueue(const std::string &qname, bool qdurable, bool qexclusive, bool qauto_delete,
                          const google::protobuf::Map<std::string, std::string> &args)
        {
            // 初始化队列消息句柄（小心存储管理）
            // 队列的创建
            _mmp->initQueueMessage(qname);
            return _mqm->declareMsgQueue(qname, qdurable, qexclusive, qauto_delete, args);
        }

        bool deleteQueue(const std::string &name)
        {
            // 删除的时候队列相关数据有俩种
            // 1、队列消息和 队列绑定信息
            _mmp->destoryQueueMessage(name);
            _bmp->removeQueueBindings(name);
            return _mqm->deleteMsgQueue(name);
        }

        bool Bind(const std::string &name, const std::string &qname, const std::string &key)
        {
            Exchange::ptr ep = _emp->selectExchange(name);
            if (ep.get() == nullptr)
            {
                LOG(LogLevel::DEBUG, "队列绑定失败，交换机不存在%s", name.c_str());
                return false;
            }
            MsgQueue::ptr mqp = _mqm->selectMsgQueue(qname);
            if (mqp.get() == nullptr)
            {
                LOG(LogLevel::DEBUG, "队列绑定失败，队列不存在%s", qname.c_str());
                return false;
            }
            return _bmp->bind(name, qname, key, ep->durable && mqp->durable);
        }

        bool unBind(const std::string &ename, const std::string &qname)
        {
            return _bmp->unbind(ename, qname);
        }

        MsgQueueBindingMap exchangBindings(const std::string &ename)
        {
            return _bmp->getExchangeBindings(ename);
        }

        bool basicPublish(const std::string &qname, BasicProperties *bp, const std::string &body)
        {
            MsgQueue::ptr mqp = _mqm->selectMsgQueue(qname);
            if (mqp.get() == nullptr)
            {
                LOG(LogLevel::DEBUG, "发布消息失败，队列%s不存在", qname.c_str());
                return false;
            }
            return _mmp->insert(qname, bp, body, mqp->durable);
        }

        Messageptr basicConsume(const std::string &qname)
        {
            return _mmp->front(qname);
        }

        void basicAck(const std::string &qname, const std::string &msgid)
        {
            _mmp->ack(qname, msgid);
        }

    private:
        std::string _host_name;
        ExchangeManager::ptr _emp;
        MsgQueueManager::ptr _mqm;
        BindingManager::ptr _bmp;
        MessageManager::ptr _mmp;
    };
} // namespace tntmq
