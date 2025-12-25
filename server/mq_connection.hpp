#include "mq_channel.hpp"

namespace tntmq
{

    class Connection
    {
    public:
        using ptr = std::shared_ptr<Connection>;
        Connection(const VirtualHost::ptr &host, ConsumerManager::ptr &cmp, const ProtobufCodecPtr &codec,
                   const muduo::net::TcpConnectionPtr &conn, const ThreadPool::ptr &pool) : _conn(conn),
                                                                                            _cmp(cmp),
                                                                                            _vhost(host),
                                                                                            _pool(pool),
                                                                                            _codec(codec),
                                                                                            _channels(std::make_shared<ChannelManager>())
        {
        }
        ~Connection()
        {
        }

        void openChannel(const openChannelRequestPtr &req)
        {
            // 1、判断信道id是否重复
            bool ret = _channels->openChannel(req->cid(), _vhost, _cmp, _codec, _conn, _pool);
            if (ret == false)
            {
                LOG(LogLevel::DEBUG, "信道创建失败，信道ID已存在%s", req->cid().c_str());
                // 信道已存在，回复失败
                return basicResponse(false, req->rid(), req->cid());
            }
            // 2、创建信道
            return basicResponse(true, req->rid(), req->cid());
            // 3、给客户端进行回复
        }

        void closeChannel(const closeChannelRequestPtr &req)
        {
            _channels->closeChannel(req->cid());
            return basicResponse(true, req->rid(), req->cid());
        }

        Channel::ptr getChannel(const std::string &cid)
        {
            return _channels->getChannel(cid);
        }

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
        muduo::net::TcpConnectionPtr _conn;
        ConsumerManager::ptr _cmp;
        VirtualHost::ptr _vhost;
        ThreadPool::ptr _pool;
        ProtobufCodecPtr _codec;
        ChannelManager::ptr _channels;
    };
    class ConnectionManager
    {
    public:
        using ptr = std::shared_ptr<ConnectionManager>;
        ConnectionManager()
        {
        }
        void newConnection(const VirtualHost::ptr &host, ConsumerManager::ptr &cmp, const ProtobufCodecPtr &codec,
                           const muduo::net::TcpConnectionPtr &conn, const ThreadPool::ptr &pool)
        {
            std::unique_lock<std::mutex> _lock(_mutex);
            // 创建连接对象
            auto it = _connections.find(conn);
            if (it != _connections.end())
            {
                return;
            }
            // 保存连接对象
            Connection::ptr connection = std::make_shared<Connection>(host, cmp, codec, conn, pool);
            _connections.insert(std::make_pair(conn, connection));
        }

        void deleteConnection(const muduo::net::TcpConnectionPtr &conn)
        {
            std::unique_lock<std::mutex> _lock(_mutex);
            _connections.erase(conn);
        }
        Connection::ptr getConnection(const muduo::net::TcpConnectionPtr &conn)
        {
            std::unique_lock<std::mutex> _lock(_mutex);
            auto it = _connections.find(conn);
            if (it != _connections.end())
            {
                return it->second;
            }
            return Connection::ptr();
        }

        ~ConnectionManager()
        {
        }

    private:
        std::mutex _mutex;
        std::unordered_map<muduo::net::TcpConnectionPtr, Connection::ptr> _connections;
    };
}