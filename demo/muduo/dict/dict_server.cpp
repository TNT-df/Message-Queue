#include "../include/muduo/net/TcpServer.h"
#include "../include/muduo/net/EventLoop.h"
#include "../include/muduo/net/TcpConnection.h"
#include <iostream>
#include <functional>
#include <unordered_map>
class TranslateServer
{
private:
    //_baseloop时epoll的事件监控，会进行描述符的事件监控，触发事件后进行io操作
    muduo::net::EventLoop _baseloop;
    //server 对象 用于设置回调函数，告诉服务器什么样的请求如何处理
    muduo::net::TcpServer _server;
    /**
     *  // 新连接建立成功以及关闭被调用
     */
    void onConnection(const muduo::net::TcpConnectionPtr &conn)
    {
        if (conn->connected() == true)
        {
            std::cout << "新连接建立成功\n";
        }
        else
        {
            std::cout << "新连接关闭\n";
        }
    }
    std::string translate(const std::string &str)
    {
        static std::unordered_map<std::string, std::string> dict_map = {
            {"hello", "你好"},
            {"Hello", "你好"},
            {"你好", "嗨"}};
        auto it = dict_map.find(str);
        if (it == dict_map.end())
        {
            return "I don't know";
        }
        return it->second;
    }

    // 通信连接收到请求时的回调函数
    void onMessage(const muduo::net::TcpConnectionPtr &coon, muduo::net::Buffer *buff, muduo::Timestamp)
    {
        // 1、从buf中把请求数据取出来
        std::string str = buff->retrieveAllAsString();
        // 2、调用translate 进行翻译
        std::string resp = translate(str);
        // 3、对客户端进行响应结果
        coon->send(resp);
    }

public:
    TranslateServer(int port) : _server(&_baseloop, muduo::net::InetAddress("0.0.0.0", port),
                                        "TranslateServer", muduo::net::TcpServer::kReusePort)
    {
        // std::bind 函数适配器，对指定函数进行参数绑定
        // 将类成员函数，设置为服务器的回调处理函数
        auto func1 = std::bind(&TranslateServer::onConnection, this, std::placeholders::_1);
        _server.setConnectionCallback(func1);
        _server.setMessageCallback(std::bind(&TranslateServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    }
    // 启动服务器
    void start()
    {
        _server.start();  // 开始事件监听
        _baseloop.loop(); // 开始事件监听，这是一个阻塞接口（死循环）
    }
};

int main()
{
    TranslateServer server(8085);
    server.start();
    return 0;
}
