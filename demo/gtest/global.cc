#include <iostream>
#include <gtest/gtest.h>
#include <unordered_map>
class MyEnvironment : public testing::Environment
{
public:
    virtual void SetUp() override
    {
        std::cout << "单元测试环境执行前初始化!!" << std::endl;
    }

    virtual void TearDown() override
    {
        std::cout << "单元测试执行完毕后清理环境" << std::endl;
    }
};

TEST(MyEnvironment, tes1)
{
    std::cout << "单元测试1\n";
}
TEST(MyEnvironment, tes2)
{
    std::cout << "单元测试2\n";
}

std::unordered_map<std::string, std::string> mymap;
class MyMapTest : public testing::Environment
{
public:
    virtual void SetUp() override
    {
        std::cout << "单元测试环境执行前初始化!!" << std::endl;
        mymap.insert(std::make_pair("hello","你好"));
        mymap.insert(std::make_pair("bye","再见"));
    }

    virtual void TearDown() override
    {
        std::cout << "单元测试执行完毕后清理环境" << std::endl;
        mymap.clear();
    }
};

TEST(MyMapTest, tes1)
{
    ASSERT_EQ(mymap.size(),2);
    mymap.erase("hello");
}
TEST(MyMapTest, tes2)
{
    ASSERT_EQ(mymap.size(),2);

}

int main(int argc, char *argv[]) // 修改这里：char* argv[] 而不是 const char* argv[]
{
    testing::InitGoogleTest(&argc, argv);
    testing::AddGlobalTestEnvironment(new MyEnvironment());
    testing::AddGlobalTestEnvironment(new MyMapTest());
    return RUN_ALL_TESTS(); // 修改这里：返回测试结果
}
