#include <iostream>
#include <thread>
#include <future>

void Add(int a, int b, std::promise<int> &prom)
{
    std::this_thread::sleep_for(std::chrono::seconds(3));
    std::cout << "加法" << std::endl;
    prom.set_value(a + b);
}
int main()
{
    std::promise<int> prom;

    std::future<int> f = prom.get_future();

    std::thread thr(Add, 11, 22, std::ref(prom));
    std::cout << "等待结果..." << std::endl;
    int res = f.get();
    std::cout << "结果为：" << res << std::endl;
    thr.join();
    return 0;
}