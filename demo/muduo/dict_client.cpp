#include "include/muduo/net/TcpClient.h"
#include "include/muduo/net/EventLoopThread.h"
#include "include/muduo/net/TcpConnection.h"
#include "include/muduo/base/CountDownLatch.h"
#include <iostream>
#include <functional>

class TranslateClient
{
public:
    TranslateClient(const std::string &ip, int sport) : _latch(1),
                                                        _client(_loopthread.startLoop(), muduo::net::InetAddress(ip, sport), "TranslateClient")
    {
        _client.setConnectionCallback(std::bind(&TranslateClient::onConnection, this, std::placeholders::_1));
        _client.setMessageCallback(std::bind(&TranslateClient::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    }
    // 连接服务器 需要等待连接建立成功后在返回
    void connect()
    {
        _client.connect();
        _latch.wait(); // 阻塞等待，直到连接建立成功
    }
    bool send(const std::string &msg)
    {
        // 连接状态正常发送
        if (_conn->connected())
        {
            _conn->send(msg);
            return true;
        }
        return false;
    }

private:
    muduo::CountDownLatch _latch;
    muduo::net::EventLoopThread _loopthread;
    muduo::net::TcpClient _client;
    muduo::net::TcpConnectionPtr _conn;

    // 连接建立成功时候的回调函数，连接建立成功后，唤醒阻塞
    void onConnection(const muduo::net::TcpConnectionPtr &conn)
    {
        if (conn->connected())
        {
            _latch.countDown(); // 唤醒主线程中的阻塞
            _conn = conn;
        }
        else
        {
            // 连接关闭
            _conn.reset();
        }
    }
    // 收到消息时候的回调函数
    void onMessage(const muduo::net::TcpConnectionPtr &coon, muduo::net::Buffer *buff, muduo::Timestamp)
    {
        std::cout << "翻译结果" << buff->retrieveAllAsString() << std::endl;
    }
};

int main()
{
    TranslateClient client("127.0.0.1", 8085);
    client.connect();
    while (1)
    {
        std::string buf;
        std::cin >> buf;
        client.send(buf);
    }
}