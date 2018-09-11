#ifndef ZSCHEDULER_H
#define ZSCHEDULER_H

#include <functional>
#include <vector>
#include <memory>
#include <chrono>

class ZSchedulerPrivate;

//!
//! \brief ZScheduler will run your task (callback) at given time.
//! All tasks are managed in a loop and will be excuted one by one.
//! 
class ZScheduler
{
public:
    ZScheduler& operator=(const ZScheduler& rhs) = delete;
    ZScheduler(const ZScheduler& rhs) = delete;

    //!
    //! \brief ZSCHEDULER cons
    //!
    ZScheduler();

    //! Destructor will call stop() automatically.
    ~ZScheduler();

    using callback = std::function<void()>;
    using time_point = std::chrono::system_clock::time_point;
    using duration = std::chrono::system_clock::duration;
    enum Type {
        Invalid = 0,
        Once,
        Interval,
        Cron
    };

    //!
    //! \brief start loop
    //!
    void start();

    //!
    //! \brief stop loop
    //!
    void stop();

    //!
    //! \brief scheduleAt Schedule a task at given time point.
    //! \param time
    //! \param func
    //! \return Task id (>0). Return 0 if failed.
    //!
    int scheduleAt(const time_point & time, callback&& func);
    //!
    //! \brief scheduleEvery Schedule a task which runs at given interval.
    //! \param interval
    //! \param func
    //! \param start Start time. Start from now if == nullptr.
    //! \param end End time.
    //! \return Task id (>0). Return 0 if failed.
    //!
    int scheduleEvery(duration interval, callback&& func,
                      const time_point* start = nullptr,
                      const time_point* end = nullptr);
    //!
    //! \brief scheduleCron Schedule a task with cron expression.
    //! \param cron
    //! \param func
    //! \param start Start time. Start from now if == nullptr.
    //! \param end End time.
    //! \return Task id (>0). Return -1 if failed (for example, invalid cron expression).
    //!
    int scheduleCron(const char* cron, callback&& func,
                     const time_point* start = nullptr,
                     const time_point* end = nullptr);

    //!
    //! \brief cancelTask Cancel task with specified id
    //! \param id
    //! \return True on success. False if no task with such id exists.
    //!
    bool cancelTask(int id);


    /*********************** Inspection APIs ********************/
    struct TaskTimer
    {
        ZScheduler::time_point time;
        ZScheduler::Type type;
    };
    //!
    //! \brief getAllTimers Get all timer's id in loop
    //! \return
    //!
    std::vector<int> getAllTimerID() const;
    //!
    //! \brief getTimer Get TaskTimer of specifed id.
    //! \param id
    //! \return
    //!
    TaskTimer getTimer(int id) const;

private:
    std::unique_ptr<ZSchedulerPrivate> d;
};

#endif // ZSCHEDULER_H
