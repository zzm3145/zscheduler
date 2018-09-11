#include <zscheduler.h>
#include <iostream>
#include <iomanip>
#include <thread>

using stdsec = std::chrono::seconds;

void print_time()
{
    time_t t = time(NULL);
    std::cout << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S");
}

void Task2()
{
    std::cout << "OK2 !";
    print_time();
    std::cout << std::endl;
}
void Task3()
{
    std::cout << "3 " ;
    print_time();
    std::cout << std::endl;
}



void Task1(ZScheduler & sch)
{
    std::cout << "OK1 ! ";
    print_time();
    std::cout << std::endl;

    auto now = std::chrono::system_clock::now();
    sch.scheduleAt(now + stdsec(1), []{ Task2(); });
    sch.scheduleAt(now + stdsec(2), []{ Task2(); });
    sch.scheduleAt(now + stdsec(3), []{ Task2(); });
}


void Task4()
{
    std::this_thread::sleep_for(stdsec(5));
    std::cout << "4" ;
    print_time();
    std::cout << std::endl;
}

int main()
{
    ZScheduler sch;
    sch.start();
    auto now = std::chrono::system_clock::now();

    print_time();

    sch.scheduleAt(now + stdsec(15), [&sch]{ Task1(sch); });
    sch.scheduleAt(now + stdsec(20), [&sch]{ Task1(sch); });
    sch.scheduleAt(now + stdsec(25), [&sch]{ Task1(sch); });

    sch.scheduleEvery(stdsec(2), [] {std::thread(Task4).detach();});

    int id = sch.scheduleEvery(stdsec(1), []{ Task3(); });
    std::this_thread::sleep_for(stdsec(5));
    sch.cancelTask(id);

    std::vector<int> ids = sch.getAllTimerID();
    ZScheduler::TaskTimer t = sch.getTimer(1);

    getchar();
    return 0;
}
