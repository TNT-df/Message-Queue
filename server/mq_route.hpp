#ifndef _M_ROUTE_H__
#define _M_ROUTE_H__

#include "../common/mq_msg.pb.h"
#include "../common/mq_helper.hpp"
#include <algorithm>

namespace tntmq
{
    class Router
    {
    public:
        static bool isLegalRoutingKey(const std::string &routing_key)
        {
            //
            for (auto const &ch : routing_key)
            {
                if (!islower(ch) || !isupper(ch) || isdigit(ch))
                {
                    continue;
                }
                else
                {
                    return false;
                }
            }
            return true;
        }

        static bool isLegalBindingey(const std::string &binging_key)
        {
            // 是否包含非法字符
            for (auto const &ch : binging_key)
            {
                if (!islower(ch) || !isupper(ch) || isdigit(ch) || ch == '*' || ch == '#')
                {
                    continue;
                }
                else
                {
                    return false;
                }
            }
            // * 和#必须独立存在
            std::vector<std::string> sub_words;
            StrHelper::split(binging_key, ".", sub_words);
            for (auto const &str : sub_words)
            {
                if (str.size() > 1 && str.find("*") != std::string::npos || str.find("#") != std::string::npos)
                {
                    return false;
                }
            }
            for (int i = 1; i < sub_words.size(); i++)
            {
                if (sub_words[i] == "*" && sub_words[i - 1] == "#" || sub_words[i] == "*" && sub_words[i - 1] == "#")
                    return false;
                if (sub_words[i] == "*" && sub_words[i - 1] == "#")
                    return false;
                if (sub_words[i] == "#" && sub_words[i - 1] == "#")
                    return false;
            }
            return true;
            // *和#不能连续存在
        }

        static bool route(ExchangeType type, const std::string &routeing_key, const std::string &binding_key)
        {
            if (type == ExchangeType::DIRECT)
            {
                return routeing_key == binding_key;
            }
            else if (type == ExchangeType::FANOUT)
                return true;
            else
            {
                std::vector<std::string> routheKeys;
                std::vector<std::string> bindKeys;
                StrHelper::split(routeing_key, ".", routheKeys);
                StrHelper::split(binding_key, ".", bindKeys);
                size_t n = routheKeys.size();
                size_t m = bindKeys.size();
                std::vector<std::vector<int>>
                    dp(n + 1, std::vector<int>(m + 1, 0));
                dp[0][0] = 1;
                for (int i = 1; i <= m; i++)
                {
                    if (bindKeys[i - 1] == "#")
                    {
                        dp[i][0] = true;
                        continue;
                    }
                    break;
                }
                for (int i = 1; i <= m; i++)
                {
                    for (int j = 1; j <= n; j++)
                    {
                        if (strcmp(routheKeys[j - 1].c_str(), bindKeys[i - 1].c_str()))
                        {
                            dp[i][j] == dp[i - 1][j - 1];
                        }
                        else if (bindKeys[i - 1] == "#")
                        {
                            dp[i][j] = dp[i][j - 1] | dp[i - 1][j];
                        }
                    }
                }
                return dp[m][n];
            }
        }

    private:
    };
} // namespace tbtmq

#endif