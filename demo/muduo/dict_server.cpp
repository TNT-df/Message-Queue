#include "include/net/TcpServer.h"
#include "include/net/EventLoop.h" 
#include "include/net/TcpConnection.h"

class TranslateServer{
private:
    muduo::net::EventLoop _baseloop;
    muduo::net::TcpServer _server;
    //新连接建立成功的回调函数
    void onConnection(const muduo::net::TcpConnectionPtr &conn){

    }
    //通信连接收到请求时的回调函数
    void onMessage(const muduo::net::TcpConnectionPtr & coon,muduo::net::Buffer *buff,muduo::Timestamp)
    {

    }
public:
    TranslateServer(int port):
    _server(&_baseloop,muduo::net::InetAddress("0.0.0.0",port),
"TranslateServer",muduo::net::TcpServer::kReusePort)
{
}
    //启动服务器
    void start();
};

int main()
{
    TranslateServer server(8085);
    server.start();
}
