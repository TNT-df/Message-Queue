#include<iostream>
#include <gtest/gtest.h>

/**
 * 断言宏 只能在单元测试宏函数使用
 *  ASSERT_   断言失败则退出
 *  EXPECT_ 断言失败继续运行
 *  
 */

TEST(test,LESS_than)
{
    int age = 20;
    EXPECT_LT(age,18);
    printf("OK!\n");
}

TEST(test,greater_than)
{
    int age = 20;
    ASSERT_LT(age,18);
    printf("OK!\n");
}

int main(int argc,char ** argv)
{
    testing::InitGoogleTest(&argc,argv);
    RUN_ALL_TESTS();
    return 0;
}  