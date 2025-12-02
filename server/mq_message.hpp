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
            FileHelper::createFile(_datafile);
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
                offset += msg_size;
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

    class QueueMessage
    {
    public:
        using ptr = std::shared_ptr<QueueMessage>;
        QueueMessage(std::string &basedir, const std::string &qname) : _qname(qname), _mapper(basedir, qname),
                                                                       _vaild_count(0), _total_count(0)

        {
        }

        bool recovery()
        {
            // 恢复历史消息
            std::unique_lock<std::mutex> lock(_mutex);
            _messages = _mapper.gc();
            for (auto &msg : _messages)
            {
                _durable_msgs.insert(std::make_pair(msg->payload().properties().id(), msg));
            }
            _vaild_count = _total_count = _messages.size();
        }

        bool insert(const BasicProperties *bp, const std::string &body, DeliveryMode delivery_mode)
        {

            // 1、构造消息对象
            Messageptr msg = std::make_shared<tntmq::Message>();
            msg->mutable_payload()->set_body(body);
            if (bp != nullptr)
            {
                msg->mutable_payload()->mutable_properties()->set_id(bp->id());
                msg->mutable_payload()->mutable_properties()->set_delivery_mode(bp->delivery_mode());
                msg->mutable_payload()->mutable_properties()->set_routing_key(bp->routing_key());
            }
            else
            {
                msg->mutable_payload()->mutable_properties()->set_id(UUIDHelper::generateUUID());
                msg->mutable_payload()->mutable_properties()->set_delivery_mode(delivery_mode);
                msg->mutable_payload()->mutable_properties()->set_routing_key("");
            }
            // 2、判断是否需要持久化
            std::unique_lock<std::mutex> lock(_mutex);
            if (msg->payload().properties().delivery_mode() == DURABLE)
            {
                msg->mutable_payload()->set_valid("1"); // 在持久化存储中表示数据有效
                // 1、进行持久化
                bool ret = _mapper.insert(msg);
                if (ret == false)
                {
                    LOG(LogLevel::DEBUG, "持久化消息: %s失败", body.c_str());
                    return false;
                }
                _durable_msgs.insert(std::make_pair(msg->payload().properties().id(), msg));
                _vaild_count += 1;
                _total_count += 1;
            }
            _messages.push_back(msg);
            return true;
        }

        // 每次删除消息后，判断是否需要垃圾回收
        bool remove(const std::string &msg_id)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            // 从待确认队列中查找消息
            auto it = _waitback_msgs.find(msg_id);
            if (it == _waitback_msgs.end())
            {
                LOG(LogLevel::INFO, "消息%s不存在", msg_id.c_str());
                return true; // 消息不存在，直接返回成功
            }
            // 根据消息持久化模式，决定是否删除持久化信息
            if (it->second->payload().properties().delivery_mode() == DURABLE)
            {
                // 1、删除持久化信息
                bool ret = _mapper.remove(it->second);
                if (ret == false)
                {
                    LOG(LogLevel::ERROR, "删除消息%s持久化信息失败", msg_id.c_str());
                    return false;
                }
                _durable_msgs.erase(msg_id);
                _vaild_count -= 1;
                gc();
            }
            // 删除内存消息
            _waitback_msgs.erase(msg_id);

            return true;
        }

        Messageptr front()
        {
            std::unique_lock<std::mutex> lock(_mutex);
            // 获取一条待推送消息
            // 将该消息对象，向待确认的hash表中添加一份，等到消息确认后再删除
            Messageptr msg = _messages.front();
            _messages.pop_front();
            _waitback_msgs.insert(std::make_pair(msg->payload().properties().id(), msg));

            return msg;
        }

        size_t getable_count()
        {
            std::unique_lock<std::mutex> lock(_mutex);
            return _messages.size();
        }

        size_t total_count()
        {
            std::unique_lock<std::mutex> lock(_mutex);
            return _total_count;
        }

        size_t durable_count()

        {
            std::unique_lock<std::mutex> lock(_mutex);
            return _durable_msgs.size();
        }

        size_t waitack_count()
        {
            std::unique_lock<std::mutex> lock(_mutex);
            return _waitback_msgs.size();
        }

        void clear()
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _mapper.removeMsgFile();
            _messages.clear();
            _durable_msgs.clear();
            _waitback_msgs.clear();
            _vaild_count = 0;
            _total_count = 0;
        }

    private:
        bool GCCheck()
        {
            // 持久化的消息总量大于2000，且其中有效比例低于50%则需要持久化
            if (_total_count > 2000 && _vaild_count * 10 / _total_count < 5)
            {
                return true;
            }
            return false;
        }

        void gc()
        {
            // 1、进行垃圾回收，获取垃圾回收后，有效消息信息链表
            if (GCCheck() == false)
                return;
            // 2、更新每一条消息的实际存储位置
            std::list<Messageptr> _messages = _mapper.gc();

            for (auto &msg : _messages)
            {
                auto it = _durable_msgs.find(msg->payload().properties().id());
                if (it == _durable_msgs.end())
                {
                    LOG(LogLevel::DEBUG, "垃圾回收后，有一条持久化消息，在内存中没有管理！");
                    _messages.push_back(msg); // 重新添加到推送链表的末尾
                    _durable_msgs.insert(std::make_pair(msg->payload().properties().id(), msg));
                    continue;
                }
                // 2、更新一条消息的实际存储位置
                it->second->set_offset(msg->offset());
                it->second->set_length(msg->length());
            }
            // 3、更新当前的有效消息数量 & 总的持久化消息数量
            _vaild_count = _total_count = _messages.size();
        }

    private:
        std::string _qname;                                         // 队列名称
        size_t _vaild_count = 0;                                    // 有效消息数量
        size_t _total_count = 0;                                    // 总的消息数量
        MessageMapper _mapper;                                      // 消息映射器
        std::list<Messageptr> _messages;                            // 待推送消息列表
        std::unordered_map<std::string, Messageptr> _durable_msgs;  // 持久化消息列表
        std::unordered_map<std::string, Messageptr> _waitback_msgs; // 待确认消息
        std::mutex _mutex;
    };

    class MessageManager
    {
    public:
        MessageManager(const std::string &basedir) : _basedir(basedir)
        {
        }

        void initQueueMessage(const std::string &qname)
        {
            QueueMessage::ptr qmp;
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _queue_msgs.find(qname);
                if (it != _queue_msgs.end())
                {
                    return;
                }
                qmp = std::make_shared<QueueMessage>(_basedir, qname);
                _queue_msgs.insert(std::make_pair(qname, qmp));
            }
            qmp->recovery();
        }

        void destoryQueueMessage(const std::string &qname)
        {
            QueueMessage::ptr qmp;
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _queue_msgs.find(qname);
                if (it != _queue_msgs.end())
                {
                    return;
                }
                qmp = it->second;
                _queue_msgs.erase(it);
            }
            qmp->clear();
        }

        Messageptr front(const std::string &qname)
        {
            QueueMessage::ptr qmp;
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _queue_msgs.find(qname);
                if (it == _queue_msgs.end())
                {
                    LOG(LogLevel::INFO, "获取队列%s队首消息，没有找到对应管理句柄", qname.c_str());
                    return Messageptr();
                }
                qmp = it->second;
            }
            return qmp->front();
        }

        bool insert(const std::string &qname, BasicProperties *bp, const std::string &body, DeliveryMode mode)
        {
            QueueMessage::ptr qmp;
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _queue_msgs.find(qname);
                if (it == _queue_msgs.end())
                {
                    LOG(LogLevel::INFO, "队列%s新增消息失败，没有找到管理句柄", qname.c_str());
                    return false;
                }
                qmp = it->second;
            }
            return qmp->insert(bp, body, mode);
        }

        void ack(const std::string &qname, const std::string msg_id)
        {
            QueueMessage::ptr qmp;
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _queue_msgs.find(qname);
                if (it == _queue_msgs.end())
                {
                    LOG(LogLevel::INFO, "确认队列%s消息%s失败，没有找到消息管理句柄", qname.c_str(), msg_id.c_str());
                    return;
                }
                qmp = it->second;
            }
            qmp->remove(msg_id);
        }

        size_t getable_count(const std::string &qname)
        {
            QueueMessage::ptr qmp;
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _queue_msgs.find(qname);
                if (it == _queue_msgs.end())
                {
                    LOG(LogLevel::DEBUG, "获取队列%s待推送消息数量失败：没有找到消息管理句柄", qname.c_str());
                    return;
                }
                qmp = it->second;
            }
            return qmp->getable_count();
        }

        size_t total_count(const std::string &qname)
        {
            QueueMessage::ptr qmp;
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _queue_msgs.find(qname);
                if (it == _queue_msgs.end())
                {
                    LOG(LogLevel::DEBUG, "获取队列%s总持久化消息数量失败：没有找到消息管理句柄", qname.c_str());
                    return;
                }
                qmp = it->second;
            }
            return qmp->total_count();
        }

        size_t durable_count(const std::string &qname)

        {
            QueueMessage::ptr qmp;
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _queue_msgs.find(qname);
                if (it == _queue_msgs.end())
                {
                    LOG(LogLevel::DEBUG, "获取队列%s有效持久化数量失败：没有找到消息管理句柄", qname.c_str());
                    return;
                }
                qmp = it->second;
            }
            return qmp->durable_count();
        }

        size_t waitack_count(const std::string &qname)
        {
            QueueMessage::ptr qmp;
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _queue_msgs.find(qname);
                if (it == _queue_msgs.end())
                {
                    LOG(LogLevel::DEBUG, "获取队列%s待确认消息数量失败：没有找到消息管理句柄", qname.c_str());
                    return;
                }
                qmp = it->second;
            }
            return qmp->waitack_count();
        }

    private:
        std::mutex _mutex;
        std::unordered_map<std::string, QueueMessage::ptr> _queue_msgs;
        std::string _basedir;
    };
};