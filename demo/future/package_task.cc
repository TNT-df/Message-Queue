#include <iostream>
#include <thread>
#include <future>
#include <memory>
// Pakcage task 示例 是一个模板类，实例化对象可以对一个函数进行二次封装
// 可以通过get_future获取一个future对象来获取封装的函数的异步执行结果

int Add(int a, int b)
{
    std::this_thread::sleep_for(std::chrono::seconds(3));
    return a + b;

}

int main()
{
    // std::packaged_task<int(int, int)> task(Add);
    // std::future<int> f = task.get_future();
    //take对象可以当做函数调用，传入Add函数需要的参数 task(11, 22),可以把task定义为指针，传递到线程，解引用执行
    // 单纯指向一个对象，存在生命期问题，可以使用智能指针std::shared_ptr
    auto ptask = std::make_shared<std::packaged_task<int(int, int)>>(Add);
    std::future<int> f = ptask->get_future();
    std::thread t([ptask]() {
        (*ptask)(11, 22); //解引用对象，执行重载的函数调用操作符
    });
    int res = f.get();
    std::cout << "结果为：" << res << std::endl;
    t.join();   
    return 0;
}