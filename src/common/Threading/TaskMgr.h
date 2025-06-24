#ifndef  TASK_SCHEDULER_H
#define  TASK_SCHEDULER_H

#include "Common.h"
#include "Timer.h"
#include <memory>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <utility>
#include <functional>

class ThreadPool
{
    using service = boost::asio::io_context;
    using pool = boost::thread_group;

public:
    ThreadPool(size_t threads)
        : _service(),
        _worker(boost::asio::executor_work_guard<service::executor_type>(_service.get_executor()))
    {
        while (threads--)
        {
            _pool.create_thread([this] { _service.run(); });
        }
    }

    void Stop()
    {
        _worker.reset();
        _pool.join_all();
        _service.stop();
    }

    template<class F>
    void Enqueue(F f)
    {
        _service.post(std::move(f));
    }

private:
    service _service;
    boost::asio::executor_work_guard<service::executor_type> _worker;
    pool _pool;
};



class TaskBase;
class CallbackTask;

//! Class for realtime task scheduling
class TaskMgr
{
public:

    ~TaskMgr();

    //! Not singleton.
    //! Just global accessor for main thread scheduler.
    static TaskMgr* Default()
    {
        static TaskMgr _default;
        return &_default;
    }

    //! Set time (in milliseconds) for executing task cycle.
    //! No garantees that it will be max time of Update() call.
    void SetMaxTickTime(uint32 ms)
    {
        _max = std::chrono::milliseconds(ms);
    }

    //! Executing task cycle.
    //! Update state of current tasks.
    //! Objects of executed tasks will be deleted.
    //! If the current work time will be longer than the _max,
    //! the cycle will be interrupted and the remaining tasks will be completed no earlier than next Update() call.
    //! Need to call this method in context which associated with scheduler.
    void Update();

    //! Add task to queue.
    //! Thread-safe.
    //! Guarantees that task will be executed not earlier than next update.
    void ScheduleInvocation(std::function<void()> func);
    void ScheduleInvocation(TaskBase* task);

    static void Async(std::function<void()> func)
    {
        _pool.Enqueue(func);
    }
    static void Stop()
    {
        _pool.Stop();
    }

private:
    typedef std::shared_ptr<TaskBase> TaskObject;
    ACE_Based::LockedQueue<TaskObject, ACE_Thread_Mutex> _queue;
    std::list<TaskObject> _tasks;
    std::chrono::milliseconds _max = std::chrono::milliseconds(25);
    static ThreadPool _pool;
};

//! Object linked to task.
//! Provides safe interaction with task.
class TaskToken
{
public:
    //! Is task canceled.
    bool IsCanceled() const
    {
        return _canceled;
    }

    //! Cancel the task.
    //! It means that continuation of task will not execute.
    void Cancel()
    {
        _canceled = true;
    }

    //! If token expired - task allready executed and token can be removed.
    bool IsExpired() const
    {
        return _expired;
    }

    //! Delink from task.
    void Delink()
    {
        _expired = true;
    }

private:
    bool _canceled = false;
    bool _expired = false;
};

typedef std::shared_ptr<TaskToken> CancelationToken;

//! Base class for objects whose actions can be scheduled.
class Schedulable
{
public:
    virtual ~Schedulable()
    {
        CancelAll();
    }

    //! Drops all active task
    void CancelAll()
    {
        for (auto&& tok : _tokens)
            tok->Cancel();
        _tokens.clear();
    }

    //! Adds a new action for a tracking and a possible safe cancelation.
    void NewAction(TaskBase* task);

private:
    std::list<CancelationToken> _tokens;
};

#endif //TASK_SCHEDULER_H
