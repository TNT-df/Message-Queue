#include "mq_exchange.hpp"
#include "mq_queue.hpp"
#include "mq_binding.hpp"
#include "mq_message.hpp"

namespace tntmq
{
    class VirtualHost
    {
    public:
        VirtualHost(const std::string &basedir, const std::string &dbfile) : _emp(std::make_shared<ExchangeManager>(dbfile)),
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
                             std::unordered_map<std::string, std::string> &args);

        void deleteExchange(const std::string &name);

        bool declareQueue(const std::string &qname, bool qdurable, bool qexclusive, bool qauto_delete,
                          std::unordered_map<std::string, std::string> &args);

        bool deleteQueue(const std::string &name);

        bool Bind(const std::string &name, const std::string &qname, const std::string &key);

        bool unBind(const std::string &ename, const std::string &qname);

        MsgQueueBindingMap exchangBindings(const std::string &ename);

        bool basicPublish(const std::string &qname, BasicProperties *bp, const std::string &body, DeliveryMode mode);

        Messageptr basicConsume(const std::string &qname);

        bool basicAck(const std::string &qname, const std::string &msgid);

    private:
        std::string _host_name;
        ExchangeManager::ptr _emp;
        MsgQueueManager::ptr _mqm;
        BindingManager::ptr _bmp;
        MessageManager::ptr _mmp;
    };
} // namespace tntmq
