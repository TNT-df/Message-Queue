#ifndef _M_CHANNEL_H__
#define _M_CHANNEL_H__
#include "muduo/net/TcpConnection.h"
#include "muduo/proto/codec.h"
#include "muduo/proto/dispatcher.h"
#include "../common/mq_logger.hpp"
#include "../common/mq_helper.hpp"
#include "../common/mq_msg.pb.h"
#include "../common/mq_proto.pb.h"
#include "../common/mq_threadPool.hpp"
#include "mq_consumer.hpp"
#include "mq_host.hpp"
#include "mq_route.hpp"
namespace tntmq
{
    using ProtobufCodecPtr = std::shared_ptr<ProtobufCodec>;
    using openChannelRequestPtr = std::shared_ptr<openChannelRequest>;
    using closeChannelRequestPtr = std::shared_ptr<closeChannelRequest>;
    using declareExchangeRequestPtr = std::shared_ptr<declareExchangeRequest>;
    using deleteExchangeRequestPtr = std::shared_ptr<deleteExchangeRequest>;
    using declareQueueRequestPtr = std::shared_ptr<declareQueueRequest>;
    using deleteQueueRequestPtr = std::shared_ptr<deleteQueueRequest>;
    using queueBindRequestPtr = std::shared_ptr<queueBindRequest>;
    using queueUnBindRequestPtr = std::shared_ptr<queueUnBindRequest>;
    using basicPublishRequestPtr = std::shared_ptr<basicPublishRequest>;
    using basicAckRequestPtr = std::shared_ptr<basicAckRequest>;
    using basicConsumeRequestPtr = std::shared_ptr<basicConsumeRequest>;
    using basicCancelRequestPtr = std::shared_ptr<basicCancelRequest>;
    class Channel
    {
    public:
        Channel(const std::string &id, const VirtualHost::ptr &host, ConsumerManager::ptr &cmp, const ProtobufCodecPtr &codec,
                const muduo::net::TcpConnectionPtr &conn, const ThreadPool::ptr &pool)
            : _cid(id),
              _conn(conn),
              _cmp(cmp),
              _vhost(host),
              _pool(pool),
              _codec(codec)
        {
        }
        ~Channel()
        {
            if (_consumer.get() != nullptr)
            {
                _cmp->remove(_consumer->tag, _consumer->qname);
            }
        }
        // 交换机声明与删除
        void declareExchange(const declareExchangeRequestPtr &req)
        {
            bool ret = _vhost->declareExchange(req->exchange_name(),
                                               static_cast<ExchangeType>(req->exchange_type()),
                                               req->durable(),
                                               req->auto_delete(),
                                               req->args());
            return basicResponse(ret, req->rid(), req->cid());
        }
        void deleteExchange(const deleteExchangeRequestPtr &req)
        {
            _vhost->deleteExchange(req->exchange_name());
            return basicResponse(true, req->rid(), req->cid());
        }

        // 队列的声明与删除
        void declareQueue(const declareQueueRequestPtr &req)
        {
            bool ret = _vhost->declareQueue(req->queue_name(),
                                            req->durable(),
                                            req->exclusive(),
                                            req->auto_delete(),
                                            req->args());
            if (ret == false)
            {
                return basicResponse(false, req->rid(), req->cid());
            }
            _cmp->initQueueConsumer(req->queue_name()); // 初始化消费者管理句柄
            return basicResponse(true, req->rid(), req->cid());
        }
        void deleteQueue(const deleteQueueRequestPtr &req)
        {
            _cmp->destroyQueueConsumer(req->queue_name()); // 销毁消费者管理句柄
            _vhost->deleteQueue(req->queue_name());
            return basicResponse(true, req->rid(), req->cid());
        }

        void bind(const queueBindRequestPtr &req)
        {
            bool ret = _vhost->Bind(req->exchange_name(),
                                    req->queue_name(),
                                    req->binding_key());
            return basicResponse(ret, req->rid(), req->cid());
        }
        void unBind(const queueUnBindRequestPtr &req)
        {
            _vhost->unBind(req->exchange_name(),
                           req->queue_name());
            return basicResponse(true, req->rid(), req->cid());
        }
        // 消息的发布
        void basicPublish(const basicPublishRequestPtr &req)
        {
            // 1.判断交换机是否存在
            auto ep = _vhost->selectExchange(req->exchange_name());
            if (ep.get() == nullptr)
            {
                return basicResponse(false, req->rid(), req->cid());
            }

            // 2.进行交换路由，判断消息可以发布到交换机绑定的那个队列中
            MsgQueueBindingMap mqbm = _vhost->exchangBindings(req->exchange_name());
            std::string routing_key;
            BasicProperties *properties = nullptr;
            if (req->has_properties())
            {
                properties = req->mutable_properties();
                routing_key = req->properties().routing_key();
            }

            for (auto &binding : mqbm)
            {
                // 3.将消息添加到队列
                if (Router::route(ep->type, routing_key, binding.second->binding_key))
                {
                    // 3.将消息添加到队列
                    _vhost->basicPublish(binding.first, properties, req->body());
                }
            }
            // 4.向线程池中添加一个消息消费任务(向指定队列的订阅者发送消息,线程池完成)
        }
        // 消息的确认
        void basicAck(const basicAckRequest &req);
        // 订阅队列消息
        void basicConsume(const basicConsumeRequest &req);
        // 取消订阅
        void basicCancel(const basicCancelRequest &req);

    private:
        void basicResponse(bool ok, const std::string &rid, const std::string &cid)
        {
            basicCommonResponse resp;
            resp.set_rid(rid);
            resp.set_cid(cid);
            resp.set_ok(ok);
            _codec->send(_conn, resp);
        }

    private:
        std::string _cid;
        Consumer::ptr _consumer;
        muduo::net::TcpConnectionPtr _conn;
        ConsumerManager::ptr _cmp;
        VirtualHost::ptr _vhost;
        ThreadPool::ptr _pool;
        ProtobufCodecPtr _codec;
    };
}
#endif