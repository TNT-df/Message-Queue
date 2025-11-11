#include "sqlite.hpp"
#include <cassert>
#include <vector>

int select_stu_callback(void *args,int col_count,char **result,char **fields_name)
{
    std::vector<std::string> * arry = (std::vector<std::string>*)args;
    arry->push_back(result[0]);
    return 0;
}
int main()
{
    // 1、创建/打开库文件

    // 3、新增信息,修改，删除，查询

    SqliteHelper helper("./student.db");
    assert(helper.open());
    // 2、创建表（不存在创建)，学生信息:学号姓名年龄
//  const char *ct = "CREATE TABLE IF NOT EXISTS student (sn INTEGER PRIMARY KEY, name VARCHAR(32), age INTEGER);";
//     assert(helper.exec(ct, nullptr, nullptr));
    // 3、新增信息,修改，删除，查询
    // const char * sql  = "select sn ,sname, age from student;"
    // const char *insert = "insert into student values(1,'tnt',15),(2,'tnt-df',15),(3,'tnt-dd',17);";
    // assert(helper.exec(insert, nullptr, nullptr));
    const char *select_sql = "select name from student";
    std::vector<std::string> array;
    assert(helper.exec(select_sql,select_stu_callback,&array));
    for(auto &name :array)
    {
        std::cout<<name <<std::endl;
    }
    //关闭数据库
    helper.close();
    return 0;
}