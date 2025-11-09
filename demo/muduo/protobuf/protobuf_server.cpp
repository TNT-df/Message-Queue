#include "muduo/proto/codec.h"
#include "muduo/proto/dispatcher.h"
#include "muduo/base/Logging.h"
#include "muduo/base/Mutex.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/TcpServer.h"

#include "request.pb.h"

#include <iostream>

class Server
{

public:
    typedef std::shared_ptr<google::protobuf::Message> MessagePtr;
    typedef std::shared_ptr<tnt::TranslateRequest> TranslateRequestPtr;
    typedef std::shared_ptr<tnt::TranslateResponse> TranslateResponsePtr;
    typedef std::shared_ptr<tnt::CalcuateRequest> CalcuateRequestPtr;
    typedef std::shared_ptr<tnt::CalcuateResponse> CalcuateResponsePtr;
    Server(int port) : _server(&_baseloop, muduo::net::InetAddress("0.0.0.0", port), "server",
                               muduo::net::TcpServer::kReusePort),
                       _dispatcher(std::bind(&Server::onUnknownMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)),
                       _codec(std::bind(&ProtobufDispatcher::onProtobufMessage, &_dispatcher, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3))
    {
        // 注册业务请求处理函数
        _dispatcher.registerMessageCallback<tnt::TranslateRequest>(std::bind(&Server::onTranslate, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
        _dispatcher.registerMessageCallback<tnt::CalcuateRequest>(std::bind(&Server::onCalculator, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
        _server.setMessageCallback(std::bind(&ProtobufCodec::onMessage, &_codec, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
        _server.setConnectionCallback(std::bind(&Server::onConnection, this, std::placeholders::_1));
    }
    void start()
    {
        _server.start();
        _baseloop.loop();
    }

private:
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
    tnt::CalcuateResponse calcuate(int num1, int num2, const std::string &op)
    {

        tnt::CalcuateResponse resp;
        resp.set_code(1);
        char x = op[0];
        if (x == '+')
        {
            resp.set_result(num1 + num2);
        }
        else if (x == '-')
        {
            resp.set_result(num1 - num2);
        }
        else if (x == '*')
        {
            resp.set_result(num1 - num2);
        }
        else
        {
            if (num2 == 0)
            {
                resp.set_code(-1);
                resp.set_result(-1);
            }
        }
        return resp;
    }

    void onTranslate(const muduo::net::TcpConnectionPtr &conn,
                     const TranslateRequestPtr &message,
                     muduo::Timestamp)
    {
        // 1、提取Message 中的有消息
        std::string req_msg = message->msg();
        // 2、进行翻译
        std::string result = translate(req_msg);
        // 3、组织protobuf 中的xiangying
        tnt::TranslateResponse resp;
        resp.set_msg(result);
        _codec.send(conn, resp);

        // 4、进行响应
    }
    void onCalculator(const muduo::net::TcpConnectionPtr &conn,
                      const CalcuateRequestPtr &message,
                      muduo::Timestamp)
    {
        int num1 = message->num1();
        int num2 = message->num2();
        std::string op = message->op();
        tnt::CalcuateResponse resp = calcuate(num1, num2, op);
        _codec.send(conn, resp);
    }
    void onConnection(const muduo::net::TcpConnectionPtr &conn)
    {
        if (conn->connected())
        {
            LOG_INFO << "新连接建立成功";
        }
        else
        {
            LOG_INFO << "连接关闭";
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
    ProtobufCodec _codec;           // protobuf协议处理器，对收到的请求数据进行protobuf协议处理
};

int main()
{
    Server Server(8080);
    Server.start();
}