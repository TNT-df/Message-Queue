#ifndef _M_CONSUMER_H__
#define _M_CONSUMER_H__
#include "../common/mq_logger.hpp"
#include "../common/mq_helper.hpp"
#include "../common/mq_msg.pb.h"
#include <iostream>
#include <mutex>
#include <memory>
#include <vector>

namespace tntdf
{
    struct Consunmer
    {
        std::string tag;
        std::string qname; // 消费者订阅的队列名称
    }
};
#endif