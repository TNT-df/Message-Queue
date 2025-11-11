#include "muduo/proto/dispatcher.h"
#include "muduo/proto/codec.h"

#include "muduo/base/Logging.h"
#include "muduo/base/Mutex.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/EventLoopThread.h"
#include "muduo/base/CountDownLatch.h"
#include "muduo/net/TcpClient.h"
#include "muduo/base/CountDownLatch.h"
#include "request.pb.h"
#include <iostream>

class Client
{
public:
    typedef std::shared_ptr<google::protobuf::Message> MessagePtr;
    typedef std::shared_ptr<tnt::TranslateResponse> TranslateResponsePtr;
    typedef std::shared_ptr<tnt::CalcuateResponse> CalcuateResponsePtr;
    Client(const std::string sip, int sport) : _latch(1), _client(_loopthread.startLoop(), muduo::net::InetAddress(sip, sport), "Client"),
                                               _dispatcher(std::bind(&Client::onUnknownMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)),
                                               _codec(std::bind(&ProtobufDispatcher::onProtobufMessage, &_dispatcher, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3))
    {
        // 注册业务请求处理函数
        _dispatcher.registerMessageCallback<tnt::TranslateResponse>(std::bind(&Client::onTranslate, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
        _dispatcher.registerMessageCallback<tnt::CalcuateResponse>(std::bind(&Client::onCalculator, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
        _client.setMessageCallback(std::bind(&ProtobufCodec::onMessage, &_codec, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
        _client.setConnectionCallback(std::bind(&Client::onConnection, this, std::placeholders::_1));
    }
    void connect()
    {
        _client.connect();
        _latch.wait(); // 阻塞等待，直到连接成功
    }
    void Translate(const std::string str)
    {
        tnt::TranslateRequest req;
        req.set_msg(str);
        send(&req);
    }
    void cal(int num1, int num2, std::string op)
    {
        tnt::CalcuateRequest req;
        req.set_num1(num1);
        req.set_num2(num2);
        req.set_op(op);
        send(&req);
    }

private:
    void onTranslate(const muduo::net::TcpConnectionPtr &conn, const TranslateResponsePtr &message, muduo::Timestamp)
    {
        std::cout << "翻译结果" << message->msg() << std::endl;
    }

    void onCalculator(const muduo::net::TcpConnectionPtr &conn, const CalcuateResponsePtr &message, muduo::Timestamp)
    {
        std::cout << "计算结果" << message->result() << std::endl;
    }
    void onUnknownMessage(const muduo::net::TcpConnectionPtr &conn, const MessagePtr &message, muduo::Timestamp)
    {
        LOG_INFO << "onUnknownMessage: " << message->GetTypeName();
        conn->shutdown();
    }

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

    bool send(const google::protobuf::Message *message)
    {
        if (_conn->connected())
        {
            _codec.send(_conn, *message);
            return true;
        }
        return false;
    }

private:
    muduo::CountDownLatch _latch;            // 实现同步
    muduo::net::EventLoopThread _loopthread; // 异步循环处理线程
    muduo::net::TcpClient _client;           // 客户端句柄
    muduo::net::TcpConnectionPtr _conn;      // 客户端连接
    ProtobufDispatcher _dispatcher;          // 请求分发器
    ProtobufCodec _codec;                    // 协议处理器
};

int main()
{
    Client client("127.0.0.1",8085);
    client.connect();
    client.Translate("hello");
    client.cal(1,2,"*");
    
    sleep(1);

    return 0;
}