#include "mq_broker.hpp"

int main()
{
    tntmq::BrokerServer server(8085, "./data");
    server.start();
    return 0;
}