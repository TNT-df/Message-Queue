#include "../server/mq_message.hpp"
#include <gtest/gtest.h>

tntmq::MessageManager::ptr mmp;

class MessageTest : public testing::Environment
{
public:
    virtual void SetUp() override
    {
        mmp = std::make_shared<tntmq::MessageManager>("./data/message/");
        mmp->initQueueMessage("q1");
    }
    virtual void TearDown() override
    {
        mmp->clear();
    }
};

// 新增消息，观察可获取消息数量，以及持久化消息数量
TEST(message_test, insert_test)
{
    tntmq::BasicProperties properties;
    properties
    mmp->insert("q1",nullptr,"Hello world-1",tntmq::DeliveryMode::DURABLE);
    mmp->insert("q2",nullptr,"Hello world-2",tntmq::DeliveryMode::DURABLE);
    mmp->insert("q3",nullptr,"Hello world-3",tntmq::DeliveryMode::DURABLE);
    mmp->insert("q4",nullptr,"Hello world-4",tntmq::DeliveryMode::DURABLE);
    mmp->insert("q5",nullptr,"Hello world-5",tntmq::DeliveryMode::DURABLE);
    mmp->insert("q6",nullptr,"Hello world-6",tntmq::DeliveryMode::DURABLE);
}

// 获取消息测试，获取一条消息，在不进行确认，以及进行确认后，查看消息数量，以及测试消息顺序的数量
TEST(message_test, get_test)
{
}

// 删除消息测试，确认一条消息 查看消息数量
TEST(message_test, delete_test)
{
}
//
int main(int argc, char *argv[])
{
    testing::InitGoogleTest(&argc, argv);
    testing::AddGlobalTestEnvironment(new MessageTest);
    RUN_ALL_TESTS();
    return 0;
}