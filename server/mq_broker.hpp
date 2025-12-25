#ifndef __M_BROKER_H__
#define __M_BROKER_H__
#include "muduo/proto/codec.h"
#include "muduo/proto/dispatcher.h"
#include "muduo/base/Logging.h"
#include "muduo/base/Mutex.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/TcpServer.h"

#include "mq_connection.hpp"
#include "mq_consumer.hpp"
#include "mq_host.hpp"
#include "../common/mq_threadPool.hpp"
#include "../common/mq_msg.pb.h"
#include "../common/mq_proto.pb.h"
#include "../common/mq_logger.hpp"

namespace tntmq
{
#define DEFILE "mq_data.db"
    class BrokerServer
    {
    public:
        typedef std::shared_ptr<google::protobuf::Message> MessagePtr;

        BrokerServer(int port, const std::string &basedir) : _server(&_baseloop, muduo::net::InetAddress("0.0.0.0", port), "server",
                                                                     muduo::net::TcpServer::kReusePort),
                                                             _dispatcher(std::bind(&BrokerServer::onUnknownMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)),
                                                             _codec(std::make_shared<ProtobufCodec>(std::bind(&ProtobufDispatcher::onProtobufMessage, &_dispatcher, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3))),
                                                             _virtual_host(std::make_shared<VirtualHost>("default", basedir, basedir + "/" + DEFILE)),
                                                             _consumer_manager(std::make_shared<ConsumerManager>()),
                                                             _connection_manager(std::make_shared<ConnectionManager>()),
                                                             _thread_pool(std::make_shared<ThreadPool>(4))
        {
            // 针对历史消息中的所有队列，初始化队列消费者管理结构
            std::unordered_map<std::string, MsgQueue::ptr> qm = _virtual_host->allQueues();
            for (const auto &q : qm)
            {
                _consumer_manager->initQueueConsumer(q.first);
            }

            // 注册业务请求处理函数
            _dispatcher.registerMessageCallback<tntmq::openChannelRequest>(std::bind(&BrokerServer::onOpenChannel, this,
                                                                                     std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
            _dispatcher.registerMessageCallback<tntmq::closeChannelRequest>(std::bind(&BrokerServer::onCloseChannel, this,
                                                                                      std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
            _dispatcher.registerMessageCallback<tntmq::declareExchangeRequest>(std::bind(&BrokerServer::onDeclareExchange, this,
                                                                                         std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
            _dispatcher.registerMessageCallback<tntmq::deleteExchangeRequest>(std::bind(&BrokerServer::onDeleteExchange, this,
                                                                                        std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
            _dispatcher.registerMessageCallback<tntmq::declareQueueRequest>(std::bind(&BrokerServer::onDeclareQueue, this,
                                                                                      std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
            _dispatcher.registerMessageCallback<tntmq::deleteQueueRequest>(std::bind(&BrokerServer::onDeleteQueue, this,
                                                                                     std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
            _dispatcher.registerMessageCallback<tntmq::queueBindRequest>(std::bind(&BrokerServer::onBindQueue, this,
                                                                                   std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
            _dispatcher.registerMessageCallback<tntmq::queueUnBindRequest>(std::bind(&BrokerServer::onUnbindQueue, this,
                                                                                     std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
            _dispatcher.registerMessageCallback<tntmq::basicPublishRequest>(std::bind(&BrokerServer::onPublish, this,
                                                                                      std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
            _dispatcher.registerMessageCallback<tntmq::basicAckRequest>(std::bind(&BrokerServer::onBasicAck, this,
                                                                                  std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
            _dispatcher.registerMessageCallback<tntmq::basicConsumeRequest>(std::bind(&BrokerServer::onBasicConsume, this,
                                                                                      std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
            _dispatcher.registerMessageCallback<tntmq::basicCancelRequest>(std::bind(&BrokerServer::onBasicCancel, this,
                                                                                     std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
            _server.setMessageCallback(std::bind(&ProtobufCodec::onMessage, _codec.get(),
                                                 std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
            _server.setConnectionCallback(std::bind(&BrokerServer::onConnection, this, std::placeholders::_1));
        }
        void start()
        {
            _server.start();
            _baseloop.loop();
        }

    private:
        // 打开信道
        void onOpenChannel(const muduo::net::TcpConnectionPtr &conn,
                           const openChannelRequestPtr &message,
                           muduo::Timestamp)
        {
            Connection::ptr connection = _connection_manager->getConnection(conn);
            if (connection.get() == nullptr)
            {
                LOG(LogLevel::DEBUG, "打开信道时，没有找到链接对应的Connection对象");
                conn->shutdown();
                return;
            }
            return connection->openChannel(message);
        }
        // 关闭信道
        void onCloseChannel(const muduo::net::TcpConnectionPtr &conn,
                            const closeChannelRequestPtr &message,
                            muduo::Timestamp)
        {
            Connection::ptr connection = _connection_manager->getConnection(conn);
            if (connection.get() == nullptr)
            {
                LOG(LogLevel::DEBUG, "关闭信道时，没有找到链接对应的Connection对象");
                conn->shutdown();
                return;
            }
            return connection->closeChannel(message);
        }
        // 声明交换机
        void onDeclareExchange(const muduo::net::TcpConnectionPtr &conn,
                               const declareExchangeRequestPtr &message,
                               muduo::Timestamp)
        {
            Connection::ptr connection = _connection_manager->getConnection(conn);
            if (connection.get() == nullptr)
            {
                LOG(LogLevel::DEBUG, "声明交换机时，没有找到链接对应的Connection对象");
                conn->shutdown();
                return;
            }
            Channel::ptr channel = connection->getChannel(message->cid());
            if (channel.get() == nullptr)
            {
                LOG(LogLevel::DEBUG, "声明交换机时，没有找到对应的Channel对象");
                conn->shutdown();
                return;
            }
            return channel->declareExchange(message);
        }
        // 删除交换机
        void onDeleteExchange(const muduo::net::TcpConnectionPtr &conn,
                              const deleteExchangeRequestPtr &message,
                              muduo::Timestamp)
        {
            Connection::ptr connection = _connection_manager->getConnection(conn);
            if (connection.get() == nullptr)
            {
                LOG(LogLevel::DEBUG, "删除交换机时，没有找到链接对应的Connection对象");
                conn->shutdown();
                return;
            }
            Channel::ptr channel = connection->getChannel(message->cid());
            if (channel.get() == nullptr)
            {
                LOG(LogLevel::DEBUG, "删除交换机时，没有找到对应的Channel对象");
                conn->shutdown();
                return;
            }
            return channel->deleteExchange(message);
        }
        // 声明队列
        void onDeclareQueue(const muduo::net::TcpConnectionPtr &conn,
                            const declareQueueRequestPtr &message,
                            muduo::Timestamp)
        {
            Connection::ptr connection = _connection_manager->getConnection(conn);
            if (connection.get() == nullptr)
            {
                LOG(LogLevel::DEBUG, "声明队列时，没有找到链接对应的Connection对象");
                conn->shutdown();
                return;
            }
            Channel::ptr channel = connection->getChannel(message->cid());
            if (channel.get() == nullptr)
            {
                LOG(LogLevel::DEBUG, "声明队列时，没有找到对应的Channel对象");
                conn->shutdown();
                return;
            }
            return channel->declareQueue(message);
        }
        // 删除队列
        void onDeleteQueue(const muduo::net::TcpConnectionPtr &conn,
                           const deleteQueueRequestPtr &message,
                           muduo::Timestamp)
        {
            Connection::ptr connection = _connection_manager->getConnection(conn);
            if (connection.get() == nullptr)
            {
                LOG(LogLevel::DEBUG, "删除队列时，没有找到链接对应的Connection对象");
                conn->shutdown();
                return;
            }
            Channel::ptr channel = connection->getChannel(message->cid());
            if (channel.get() == nullptr)
            {
                LOG(LogLevel::DEBUG, "删除队列时，没有找到对应的Channel对象");
                conn->shutdown();
                return;
            }
            return channel->deleteQueue(message);
        }
        // 队列绑定
        void onBindQueue(const muduo::net::TcpConnectionPtr &conn,
                         const queueBindRequestPtr &message,
                         muduo::Timestamp)
        {
            Connection::ptr connection = _connection_manager->getConnection(conn);
            if (connection.get() == nullptr)
            {
                LOG(LogLevel::DEBUG, "队列绑定时，没有找到链接对应的Connection对象");
                conn->shutdown();
                return;
            }
            Channel::ptr channel = connection->getChannel(message->cid());
            if (channel.get() == nullptr)
            {
                LOG(LogLevel::DEBUG, "队列绑定时，没有找到对应的Channel对象");
                conn->shutdown();
                return;
            }
            return channel->bind(message);
        }
        // 队列解绑
        void onUnbindQueue(const muduo::net::TcpConnectionPtr &conn,
                           const queueUnBindRequestPtr message,
                           muduo::Timestamp)
        {
            Connection::ptr connection = _connection_manager->getConnection(conn);
            if (connection.get() == nullptr)
            {
                LOG(LogLevel::DEBUG, "队列解绑时，没有找到链接对应的Connection对象");
                conn->shutdown();
                return;
            }
            Channel::ptr channel = connection->getChannel(message->cid());
            if (channel.get() == nullptr)
            {
                LOG(LogLevel::DEBUG, "队列解绑时，没有找到对应的Channel对象");
                conn->shutdown();
                return;
            }
            return channel->unBind(message);
        }
        // 消息发布
        void onPublish(const muduo::net::TcpConnectionPtr &conn,
                       const basicPublishRequestPtr &message,
                       muduo::Timestamp)
        {
            Connection::ptr connection = _connection_manager->getConnection(conn);
            if (connection.get() == nullptr)
            {
                LOG(LogLevel::DEBUG, "消息发布时，没有找到链接对应的Connection对象");
                conn->shutdown();
                return;
            }
            Channel::ptr channel = connection->getChannel(message->cid());
            if (channel.get() == nullptr)
            {
                LOG(LogLevel::DEBUG, "消息发布时，没有找到对应的Channel对象");
                conn->shutdown();
                return;
            }
            return channel->basicPublish(message);
        }
        // 消息确认
        void onBasicAck(const muduo::net::TcpConnectionPtr &conn,
                        const basicAckRequestPtr &message,
                        muduo::Timestamp)
        {
            Connection::ptr connection = _connection_manager->getConnection(conn);
            if (connection.get() == nullptr)
            {
                LOG(LogLevel::DEBUG, "消息确认时，没有找到链接对应的Connection对象");
                conn->shutdown();
                return;
            }
            Channel::ptr channel = connection->getChannel(message->cid());
            if (channel.get() == nullptr)
            {
                LOG(LogLevel::DEBUG, "消息确认时，没有找到对应的Channel对象");
                conn->shutdown();
                return;
            }
            return channel->basicAck(message);
        }
        // 队列消息订阅
        void onBasicConsume(const muduo::net::TcpConnectionPtr &conn,
                            const basicConsumeRequestPtr &message,
                            muduo::Timestamp)
        {
            Connection::ptr connection = _connection_manager->getConnection(conn);
            if (connection.get() == nullptr)
            {
                LOG(LogLevel::DEBUG, "队列消息订阅时，没有找到链接对应的Connection对象");
                conn->shutdown();
                return;
            }
            Channel::ptr channel = connection->getChannel(message->cid());
            if (channel.get() == nullptr)
            {
                LOG(LogLevel::DEBUG, "队列消息订阅时，没有找到对应的Channel对象");
                conn->shutdown();
                return;
            }
            return channel->basicConsume(message);
        }
        // 队列消息取消订阅
        void onBasicCancel(const muduo::net::TcpConnectionPtr &conn,
                           const basicCancelRequestPtr &message,
                           muduo::Timestamp)
        {
            Connection::ptr connection = _connection_manager->getConnection(conn);
            if (connection.get() == nullptr)
            {
                LOG(LogLevel::DEBUG, "队列消息取消订阅时，没有找到链接对应的Connection对象");
                conn->shutdown();
                return;
            }
            Channel::ptr channel = connection->getChannel(message->cid());
            if (channel.get() == nullptr)
            {
                LOG(LogLevel::DEBUG, "队列消息取消订阅时，没有找到对应的Channel对象");
                conn->shutdown();
                return;
            }
            return channel->basicCancel(message);
        }
        void onConnection(const muduo::net::TcpConnectionPtr &conn)
        {
            if (conn->connected())
            {
                LOG_INFO << "新连接建立成功";
                _connection_manager->newConnection(_virtual_host, _consumer_manager,
                                                   _codec,
                                                   conn, _thread_pool);
            }
            else
            {
                LOG_INFO << "连接关闭";
                _connection_manager->deleteConnection(conn);
            }
        }

        void onUnknownMessage(const muduo::net::TcpConnectionPtr &conn,
                              const MessagePtr &message,
                              muduo::Timestamp)
        {
            LOG_INFO << "onUnknownMessage: " << message->GetTypeName();
            conn->shutdown();
        }

    private:
        muduo::net::EventLoop _baseloop;
        muduo::net::TcpServer _server;  // 服务器对象
        ProtobufDispatcher _dispatcher; // 请求分发器 -- 注册请求处理函数
        ProtobufCodecPtr _codec;        // protobuf协议处理器，对收到的请求数据进行protobuf协议处理
        VirtualHost::ptr _virtual_host;
        ConsumerManager::ptr _consumer_manager;
        ConnectionManager ::ptr _connection_manager;
        ThreadPool::ptr _thread_pool;
    };
}
#endif