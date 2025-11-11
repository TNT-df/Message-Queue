#include <iostream>
#include <gtest/gtest.h>
#include <unordered_map>

class Mytest : public testing::Test
{
public:
    static void SetupTestCase()
    {
        // 初始化公共的测试数据
        std::cout << "SetupTestCase" << std::endl;
    }
    void SetUp() override
    {
        // 插入每个单元测试所需的独立的测试数据
        _map.insert(std::make_pair("hello", "你好"));
        _map.insert(std::make_pair("bye", "再见"));
    }
    void TearDown() override
    {
        // 清理每个单元测试自己插入的测试数据
        _map.clear();
    }

    static void TearDownTestCase()
    {
        //清理公共的测试数据
        std::cout << "TearDownTestCase" << std::endl;
    }

public:
    std::unordered_map<std::string, std::string> _map;
};

TEST_F(Mytest, insert_test)
{
    _map.insert(std::make_pair("hello", "你好"));
}

TEST_F(Mytest, size_test)
{
    ASSERT_EQ(_map.size(), 2);
}

int main(int argc, char *argv[])
{

    testing::InitGoogleTest(&argc, argv);
    RUN_ALL_TESTS();
    return 0;
}
