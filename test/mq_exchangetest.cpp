#include "../server/mq_exchange.hpp"
#include <gtest/gtest.h>

tntmq::ExchangeManager::ptr emp;

class ExchangeManagerTest : public ::testing::Environment
{
public:
    virtual void SetUp() override
    {
        emp = std::make_shared<tntmq::ExchangeManager>("./data/meta.db");
    }
    virtual void TearDown() override
    {
        // emp->clear();
        // 

        std::cout << "清理" << std::endl;
    }
};

TEST(exchange_test, insert_test)
{
    std::unordered_map<std::string, std::string> map = {
        {"key1", "val1"},
        {"key2", "val2"}};
    emp->declareExchange("ex1", tntmq::ExchangeType::DIRECT, true, false, map);
    emp->declareExchange("ex2", tntmq::ExchangeType::DIRECT, true, false, map);
    emp->declareExchange("ex3", tntmq::ExchangeType::DIRECT, true, false, map);
    ASSERT_EQ(emp->size(), 3);
}

TEST(exchange_test, select_test)
{
    tntmq::Exchange::ptr exp = emp->selectExchange("ex1");
    ASSERT_NE(exp.get(), nullptr);
    ASSERT_EQ(exp->name, "ex1");
    ASSERT_EQ(exp->type, tntmq::ExchangeType::DIRECT);
    ASSERT_EQ(exp->durable, true);
    ASSERT_EQ(exp->auto_delete, false);
    ASSERT_EQ(exp->getArgs(), std::string("key1=val1&key2=val2"));
}

TEST(exchange_test, remove_test)
{
    emp->deleteExchange("ex1");
    tntmq::Exchange::ptr exp =  emp->selectExchange("ex1");
    ASSERT_EQ(exp.get(), nullptr);
}
int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    testing::AddGlobalTestEnvironment(new ExchangeManagerTest());
    RUN_ALL_TESTS();
    return 0;
} 