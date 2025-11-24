#include "../common/mq_helper.hpp"
#include <gtest/gtest.h>

int main(int argc, char **argv)
{
    // tntmq::FileHelper helper("../common/logger.hpp");
    // LOG(tntmq::LogLevel::INFO, "file exists:%d", helper.exists());
    // LOG(tntmq::LogLevel::INFO, "file size:%ld", helper.size());

    // tntmq::FileHelper tep("./aaa/bbb/ccc/tmp.hpp");
    // if (tep.exists() == false)
    // {
    //     std::string dir = tntmq::FileHelper::parentDirectory("aaa/bbb/ccc/tmp.hpp");
    //     if (tntmq::FileHelper(dir).exists() == false)
    //     {
    //         tntmq::FileHelper::createDirectory(dir);
    //     }
    //     tntmq::FileHelper::createFile("aaa/bbb/ccc/tmp.hpp");
    // }

    // std::string body;
    // helper.read(body);
    // std::cout << body << std::endl;
    // tep.write(body);

    // tntmq::FileHelper tep("./aaa/bbb/ccc/tmp.hpp");
    // char str[16] = {0};
    // tep.read(str, 8, 11);
    // tep.write("HELLO WORLD", 8, 11);
    // // tntmq::FileHelper::removeFile("./aaa/bbb/ccc/tmp.hpp");
    tntmq::FileHelper::removeDirectory("./aaa");
    return 0;
}