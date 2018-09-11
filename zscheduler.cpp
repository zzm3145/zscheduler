#include <mutex>
#include <thread>
#include <map>
#include <condition_variable>
#include <memory>
#include <string.h>
#include <utility>
#include "ccronexpr.h"
#include "zscheduler.h"

using sysclock = std::chrono::system_clock;

class TaskFunctionTimer
{
public:
    TaskFunctionTimer () {}

    TaskFunctionTimer(ZScheduler::callback&& f,
                      const ZScheduler::time_point& t)
        : func_(f), time_(t), type_(ZScheduler::Once)
    {
    }

    TaskFunctionTimer(ZScheduler::callback&& f,
                      const ZScheduler::duration& i,
                      const ZScheduler::time_point *start,
                      const ZScheduler::time_point *end)
        : func_(f), interval_(i), type_(ZScheduler::Interval)
    {
        setRange(start, end);

        if (!this->startTime_)
            this->startTime_.reset(new ZScheduler::time_point(sysclock::now()));
        this->time_ = *this->startTime_ + interval_; //first exec
    }

    TaskFunctionTimer(ZScheduler::callback&& f,
                      cron_expr&& c,
                      const ZScheduler::time_point *start,
                      const ZScheduler::time_point *end)
        : func_(f), cron_(std::move(c)), type_(ZScheduler::Cron)
    {
        setRange(start, end);

        time_t next = cron_next(&cron_, sysclock::to_time_t(sysclock::now()));
        this->time_ = sysclock::from_time_t(next);
    }

    void setRange(const ZScheduler::time_point *start,
                  const ZScheduler::time_point *end)
    {
        if (start)
            startTime_.reset(new ZScheduler::time_point(*start));
        if (end)
            endTime_.reset(new ZScheduler::time_point(*end));
    }

    TaskFunctionTimer& operator=(const TaskFunctionTimer& ) = delete;
    TaskFunctionTimer(const TaskFunctionTimer& ) = delete;

    TaskFunctionTimer(TaskFunctionTimer&& ) = default;
    TaskFunctionTimer& operator=(TaskFunctionTimer&& ) = default;

    //    Use < operator if to be used with std::set.
    //    I do not use set here, since std::set always returns const reference (to protect its key order).
    //    bool operator<(const TaskFunctionTimer& rhs) const
    //    {
    //        return time > rhs.time;
    //    }

    bool exec()
    {
        bool hasNext = false;

        auto now = sysclock::now();
        if (type_ == ZScheduler::Interval)
        {
            ZScheduler::time_point temp = now + interval_;
            if (endTime_ && temp > *endTime_) {

            } else {
                time_ = temp;
                hasNext = true;
            }
        }
        else if (type_ == ZScheduler::Cron)
        {
            time_t next = cron_next(&cron_, sysclock::to_time_t(now));
            if (next != -1)
            {
                ZScheduler::time_point temp = sysclock::from_time_t(next);
                if (endTime_ && temp > *endTime_) {

                } else {
                    time_ = temp;
                    hasNext = true;
                }
            }
        }

        if (func_)
        {
            if (startTime_ && now < *startTime_)
            {
            }
            else
                func_();
        }

        return hasNext;
    }

    ZScheduler::callback func_;
    ZScheduler::time_point time_;

    std::unique_ptr<ZScheduler::time_point> startTime_;
    std::unique_ptr<ZScheduler::time_point> endTime_;

    ZScheduler::duration interval_;
    cron_expr cron_;

    ZScheduler::Type type_ = ZScheduler::Invalid;
    int id_ = 0;
};

class ZSchedulerPrivate
{
public:
    using TaskLock = std::unique_lock<std::mutex>;
    using TaskMap = std::multimap<ZScheduler::time_point, TaskFunctionTimer>;

    ZSchedulerPrivate():
        go_on(true)
    {
    }

    ~ZSchedulerPrivate()
    {
        stop();
    }

    void start()
    {
        // already has a runing loop, return
        if (thread && thread->joinable())
            return;

        thread.reset(new std::thread([this](){ this->ThreadLoop(); }));
    }

    void stop()
    {
        if (thread && thread->joinable())
        {
            go_on = false;
            schedule(TaskFunctionTimer([](){}, ZScheduler::time_point()));
            thread->join();
        }
    }

    int schedule(TaskFunctionTimer&& funcTimer)
    {
        TaskLock lock(mutex);

        static int id = 0;
        if (funcTimer.id_ == 0)
        {
            ++id;
            if (id < 0)
                id = 1;
            funcTimer.id_ = id;
        }

        auto it = tasks.emplace(funcTimer.time_, std::move(funcTimer));
        if (it == tasks.begin())
            blocker.notify_one();

        return id;
    }

    bool cancel(int id)
    {
        TaskLock lock(mutex);
        for(auto it = tasks.begin();
            it != tasks.end();
            ++it)
        {
            if (it->second.id_ == id)
            {
                tasks.erase(it);
                return true;
            }
        }
        return false;
    }

    void ThreadLoop()
    {
        while(go_on)
        {
            TaskFunctionTimer todo;
            {
                TaskLock lock(mutex);
                auto it = tasks.begin();
                if (!tasks.empty() && it->first <= sysclock::now())
                {
                    todo = std::move(it->second);
                    tasks.erase(it);
                }
            }

            // Run tasks while unlocked so tasks can schedule new tasks
            if (todo.exec())
            {
                schedule(std::move(todo));  // if has next trigger point, schedule it.
            }

            {
                TaskLock lock(mutex);
                if (tasks.empty())
                    blocker.wait(lock);
                else if (tasks.begin()->first <= sysclock::now())
                    continue;
                else
                    blocker.wait_until(lock, tasks.begin()->first);
            }
        }
    }

    TaskMap tasks;
    std::mutex mutex;
    std::unique_ptr<std::thread> thread;
    std::condition_variable blocker;
    bool go_on;
};

ZScheduler::ZScheduler()
{
    d.reset(new ZSchedulerPrivate());
}

ZScheduler::~ZScheduler()
{
}

void ZScheduler::start()
{
    d->start();
}

void ZScheduler::stop()
{
    d->stop();
}

int ZScheduler::scheduleAt(const time_point &time,
                           callback&& func)
{
    return d->schedule(TaskFunctionTimer(std::move(func), time));
}

int ZScheduler::scheduleEvery(duration interval,
                              callback&& func,
                              const time_point *start,
                              const time_point *end)
{
    return d->schedule(TaskFunctionTimer(std::move(func), interval, start, end));
}

int ZScheduler::scheduleCron(const char *cron,
                             ZScheduler::callback &&func,
                             const time_point *start,
                             const time_point *end)
{
    cron_expr expr;
    memset(&expr, 0, sizeof(expr));
    const char* err = NULL;
    cron_parse_expr(cron, &expr, &err);
    if (err) /* invalid expression */
    {
        return -1;
    }

    return d->schedule(TaskFunctionTimer(std::move(func), std::move(expr), start, end));
}

bool ZScheduler::cancelTask(int id)
{
    return d->cancel(id);
}

std::vector<int> ZScheduler::getAllTimerID() const
{
    std::vector<int> timers;
    {
        ZSchedulerPrivate::TaskLock lock(d->mutex);
        for(const auto& it : d->tasks)
        {
            timers.push_back(it.second.id_);
        }
    }
    return timers;
}

ZScheduler::TaskTimer ZScheduler::getTimer(int id) const
{
    {
        ZSchedulerPrivate::TaskLock lock(d->mutex);
        for(const auto& it : d->tasks)
        {
            if (id == it.second.id_)
                return ZScheduler::TaskTimer{it.second.time_, it.second.type_};
        }
    }
    return ZScheduler::TaskTimer{ZScheduler::time_point(), ZScheduler::Invalid};
}
