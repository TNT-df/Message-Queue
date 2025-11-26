#include "../server/mq_queue.hpp"
#include <gtest/gtest.h>

tntmq::MsgQueueManager::ptr emp;

class ExchangeManagerTest : public ::testing::Environment
{
public:
    virtual void SetUp() override
    {
        emp = std::make_shared<tntmq::MsgQueueManager>("./data/meta.db");
    }
    virtual void TearDown() override
    {


        std::cout << "清理" << std::endl;
    }
};

TEST(queue_test, insert_test)
{
    std::unordered_map<std::string, std::string> map = {
        {"key1", "val1"},
        {"key2", "val2"}};
    emp->declareMsgQueue("ex1", true, true, false, map);
    emp->declareMsgQueue("ex2", true, true, false, map);
    emp->declareMsgQueue("ex3", true, true, false, map);
    ASSERT_EQ(emp->size(), 3);
}
TEST(queue_test, select_test)
{
    tntmq::MsgQueue::ptr exp = emp->selectMsgQueue("ex1");
    ASSERT_NE(exp.get(), nullptr);
    ASSERT_EQ(exp->name, "ex1");
    ASSERT_EQ(exp->durable, true);
    ASSERT_EQ(exp->exclusive, true);
    ASSERT_EQ(exp->auto_delete, false);
    ASSERT_EQ(exp->getArgs(), std::string("key1=val1&key2=val2"));
}

TEST(queue_test, remove_test)
{
    emp->deleteMsgQueue("ex1");
    tntmq::MsgQueue::ptr exp = emp->selectMsgQueue("ex1");
    ASSERT_EQ(exp.get(), nullptr);
}
int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    testing::AddGlobalTestEnvironment(new ExchangeManagerTest());
    RUN_ALL_TESTS();
    return 0;
}