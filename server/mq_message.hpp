#include "../common/mq_logger.hpp"
#include "../common/mq_helper.hpp"
#include "../common/mq_msg.pb.h"
#include <iostream>
#include <mutex>
#include <memory>
#include <sstream>
#include <unordered_map>
#include <list>
namespace tntmq
{
#define DATAFILE_SUFFIX ".mqd"
#define TMPFILE_SUFFIX ".mqd.tmp"
    using Messageptr = std::shared_ptr<tntmq::Message>;

    class MessageMapper
    {
    public:
        MessageMapper(std::string &basedir, const std::string &qname) : _qname(qname)
        {
            if (basedir.back() != '/')
            {
                basedir.push_back('/');
            }
            _datafile = basedir + qname + DATAFILE_SUFFIX;
            _tmpfile = basedir + qname + TMPFILE_SUFFIX;
            assert(FileHelper::createDirectory(basedir));
        }

        bool createMsgFile()
        {
            bool ret = FileHelper::createFile(_datafile);
            if (ret == false)
            {
                LOG(LogLevel::ERROR, "创建消息文件%s失败", _datafile.c_str());
                return false;
            }
            return true;
        }

        void removeMsgFile()
        {
            FileHelper::removeFile(_datafile);
            FileHelper::removeFile(_tmpfile);
        }

        bool insert(Messageptr &msg)
        {
            return insert(_datafile, msg);
        }

        bool remove(Messageptr &msg)
        {
            // 1、将msg标志位修改为'0'
            msg->mutable_payload()->set_valid("0");
            // 2、对msg进行序列化
            std::string body = msg->payload().SerializeAsString();
            if (msg->length() != body.size())
            {
                LOG(LogLevel::DEBUG, "不能修改文件的数据信息，长度不一致");
                return false;
            }
            // 3、将序列化后的消息，写入到数据在文件中的制定位置
            FileHelper helper(_datafile);
            size_t fsize = helper.size();
            // 3、将数据写入文件指定位置
            bool ret = helper.write(body.c_str(), msg->offset(), body.size());
            if (ret == false)
            {
                LOG(LogLevel::ERROR, "写入消息到文件%s失败", _datafile.c_str());
                return false;
            }
            return true;
        }

        std::list<Messageptr> gc()
        {
            // 1、加载出文件中所有有效数据 存储格式为4字节长度|数据 4字节长度
            std::list<Messageptr> result;
            bool ret = load(result);
            if (ret == false)
            {
                LOG(LogLevel::DEBUG, "加载消息文件%s失败", _datafile.c_str());
                return result; // 返回空列表
            }
            // 2、将有效数据，进行序列化存储到临时文件
            for (auto &msg : result)
            {
                bool ret = insert(_tmpfile, msg);
                if (ret == false)
                {
                    LOG(LogLevel::DEBUG, "写入消息到临时文件失败", _tmpfile.c_str());
                    return result; // 返回已处理的有效数据
                }
            }
            // 3、删除源文件
            bool ret = FileHelper::removeFile(_datafile);
            if (ret == false)
            {
                LOG(LogLevel::ERROR, "删除消息文件%s失败", _datafile.c_str());
                return result; // 返回已处理的有效数据
            }

            // 4、临时修改文件名，为原文件名
            ret = FileHelper(_tmpfile).rename(_datafile);
            if (ret == false)
            {
                LOG(LogLevel::ERROR, "重命名临时文件%s失败", _tmpfile.c_str());
                return result; // 返回已处理的有效数据
            }
            // 5、返回有效数据列表
            return result;
        }

    private:
        bool insert(const std::string &filename, Messageptr &msg)
        {
            // 新增数据添加到文件末尾
            // 1、进行消息序列化
            std::string body = msg->payload().SerializeAsString();
            // 2、获取文件长度
            FileHelper helper(filename);
            size_t fsize = helper.size();
            // 3、将数据写入文件指定位置
            bool ret = helper.write(body.c_str(), fsize, body.size());
            if (ret == false)
            {
                LOG(LogLevel::ERROR, "写入消息到文件%s失败", filename.c_str());
                return false;
            }
            // 4、更新msg的实际存储信息
            msg->set_offset(fsize);
            msg->set_length(body.size());
            return true;
        }
        bool load(std::list<Messageptr> &result)
        {
            size_t msg_size;
            FileHelper data_helper(_datafile);
            size_t fsize = data_helper.size();
            size_t offset = 0;

            while (offset < fsize)
            {
                // 1、读取4字节长度
                bool ret = data_helper.read((char *)&msg_size, offset, 4);
                if (ret == false)
                {
                    LOG(LogLevel::ERROR, "读取消息文件%s失败", _datafile.c_str());
                    return false;
                }
                offset += 4;
                std::string msg_body(msg_size, '\0');
                data_helper.read(&msg_body[0], offset, msg_size);
                if (ret == false)
                {
                    LOG(LogLevel::DEBUG, "读取消息文件%s失败", _datafile.c_str());
                    return false;
                }
                offset = msg_size;
                // 2、反序列化消息内容
                Messageptr msg = std::make_shared<tntmq::Message>();
                msg->ParseFromString(msg_body);
                // 无效消息 直接处理下一个
                if (msg->payload().valid() == "0")
                {
                    continue;
                }
                // 保存有效消息
                result.push_back(msg);
            }
            return true;
        }

    private:
        std::string _qname;
        std::string _datafile;
        std::string _tmpfile;
    };
};