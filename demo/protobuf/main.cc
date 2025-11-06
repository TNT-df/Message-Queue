 #include "contacts.pb.h"
 #include<iostream>
 int main()
 {
    contacts::contact contact;
    contact.set_sn(10001);
    contact.set_name("df");
    contact.set_score(60.5);
    //持久化的数据可以放在str 对象，或者进行网络传输
    std::string str = contact.SerializeAsString();

    contacts::contact student;
    bool ret = student.ParseFromString(str);
    if(ret == false)
    {
        std::cout <<"序列化/反序列化失败\n;";
        return -1;
    }
    std::cout<<student.name()<<std::endl;
    std::cout<<student.score()<<std::endl;
    std::cout<<student.sn()<<std::endl;
    return 0;
 }