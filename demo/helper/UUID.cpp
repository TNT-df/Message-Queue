/**
 * 生成8个0-255之间的随机数
 * 去一个8字节的序号
 * 组成16字节数据，转成16进制字符，共32位
 */
#include <iostream>
#include <random>
#include <iomanip>
#include <sstream>
#include <atomic>
class UUIDHelper
{
public:
    static std::string generateUUID()
    {
        std::random_device rd;

        std::mt19937_64 gen(rd()); // 梅森旋转算法，生成伪随机数

        // 限定数据区间
        std::uniform_int_distribution<int> distribution(0, 255);

        // 将生成的数字转为16进制数字字符
        std::stringstream ss;
        for (int i = 0; i < 8; i++)
        {
            ss << std::setw(2) << std::setfill('0') << std::hex << distribution(gen);
            if (i == 3 || i == 5 || i == 7)
            {
                ss << "-";
            }
            // 定义原子类型整数初始化为1
        }
        static std::atomic<size_t> seq(1);
        size_t s = seq.fetch_add(1);
        for (int i = 7; i >= 0; i--)
        {
            ss << std::setw(2) << std::setfill('0') << std::hex << ((s >> i * 8) & 0xFF);
            if (i == 6)
                ss << "-";
        }
        return ss.str();
    }
};