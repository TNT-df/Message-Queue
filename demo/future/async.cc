#include <iostream>
#include <thread>
#include <future>

int Add(int a, int b)
{
    std::cout << "加法" << std::endl;
    return a + b;
}

int main()
{
    // std::async(func, args...)  
    //std::async(plicy, func, args...)
    std::cout << "-----------1" << std::endl;
    //执行get获取异步结果时，才会去执行Add函数
    //std::launch::deferred 延迟调用  std::launch::async 内部创建工作线程，异步完成任务
    std::future<int> res = std::async(std::launch::deferred, Add, 1, 2);
   std::this_thread::sleep_for(std::chrono::seconds(3));
    std::cout << "-----------2" << std::endl;
 
    int sum = res.get();
    
    std::cout << "-----------3" << std::endl;
    std::cout << sum << std::endl;
    return 0;
}