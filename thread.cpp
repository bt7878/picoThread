#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "pico/sync.h"
#include <exception>
#include <functional>
#include <iostream>
#include <memory>
#include <queue>
#include <system_error>
#include <type_traits>

namespace picoThread
{

class thread
{
  public:
    class id
    {
        static uint32_t _nextId;
        uint32_t _id;

      public:
        id() noexcept
        {
            _id = _nextId++;
        }

        bool operator==(id other) noexcept
        {
            return _id == other._id;
        }
        bool operator!=(id other) noexcept
        {
            return _id != other._id;
        }
        bool operator<(id other) noexcept
        {
            return _id < other._id;
        }
        bool operator>(id other) noexcept
        {
            return _id > other._id;
        }
        bool operator<=(id other) noexcept
        {
            return _id <= other._id;
        }
        bool operator>=(id other) noexcept
        {
            return _id >= other._id;
        }
        friend std::ostream &operator<<(std::ostream &out, const id &id)
        {
            out << id._id;
            return out;
        }
        friend std::hash<id>;
    };

  private:
    static id baseThreadId;

    struct _callQueue_t
    {
        id threadId;
        std::function<void()> f;
        std::shared_ptr<bool> finished_ptr;
        _callQueue_t(id _threadId, std::function<void()> _f, std::shared_ptr<bool> _finished_ptr)
        {
            threadId = _threadId;
            f = _f;
            finished_ptr = _finished_ptr;
        }
    };

    static bool _core1Started;
    static std::queue<_callQueue_t> _callQueue;

    static void _core1_entry()
    {
        while (true)
        {
            while (_callQueue.empty())
            {
                tight_loop_contents();
            }
            _callQueue_t toDo = _callQueue.front();
            std::invoke(toDo.f);
            *(toDo.finished_ptr) = true;
            _callQueue.pop();
        }
    }

    id _id = id();
    std::shared_ptr<bool> _finished_ptr = std::shared_ptr<bool>(new bool(false));
    bool _joinable = false;

  public:
    template <class Callable, class... Args> explicit thread(Callable &&f, Args &&...args)
    {
        static_assert(std::is_invocable<typename std::decay<Callable>::type, typename std::decay<Args>::type...>::value,
                      "picoThread::thread arguments must be invocable after conversion to rvalues");

        if (!_core1Started)
        {
            multicore_launch_core1(this->_core1_entry);
            multicore_fifo_push_blocking(1);
            _core1Started = true;
            std::cout << "started" << std::endl;
        }
        _joinable = true;
        std::function<void()> binded_f = std::bind(f, args...);
        _callQueue.push(_callQueue_t(_id, binded_f, _finished_ptr));
    }

    ~thread()
    {
        if (_joinable)
        {
            std::terminate();
        }
    }

    bool joinable() const noexcept
    {
        return _joinable;
    }

    static unsigned int hardware_concurrency() noexcept
    {
        return 2;
    }

    void join()
    {
        if (!_joinable)
        {
            throw std::system_error(std::make_error_code(std::errc::invalid_argument));
        }
        else if (multicore_fifo_rvalid())
        {
            throw std::system_error(std::make_error_code(std::errc::resource_deadlock_would_occur));
        }
        while (!(*_finished_ptr))
        {
            tight_loop_contents();
        }
        _joinable = false;
    }

    void detach()
    {
        if (!_joinable)
        {
            throw std::system_error(std::make_error_code(std::errc::invalid_argument));
        }
        _joinable = false;
    }

    id get_id() noexcept
    {
        return _id;
    }

    void swap(thread &other) noexcept
    {
        id tempid = _id;
        _id = other._id;
        other._id = tempid;

        std::shared_ptr<bool> tempFinishedPtr = _finished_ptr;
        _finished_ptr = other._finished_ptr;
        other._finished_ptr = tempFinishedPtr;

        bool tempJoinable = _joinable;
        _joinable = other._joinable;
        other._joinable = tempJoinable;
    }
};

// initialize static variables
uint32_t thread::id::_nextId = 0;
bool thread::_core1Started = false;
std::queue<thread::_callQueue_t> thread::_callQueue;
thread::id thread::baseThreadId = thread::id();

} // namespace picoThread

namespace std
{
template <> class hash<picoThread::thread::id>
{
  public:
    size_t operator()(const picoThread::thread::id &id)
    {
        return std::hash<string>{}(to_string(id._id));
    }
};
} // namespace std

void f(int &a)
{
    a = 10;
}

int f1(int a, int b)
{
    sleep_ms(5000);
    std::cout << "Call f1: " << a << " " << b << std::endl;
    return 0;
}

void createThreads()
{
    int b = 5;
    picoThread::thread ta(f, std::ref(b));
    picoThread::thread tb(f1, 1, 2);
    std::cout << (ta.get_id() < tb.get_id()) << std::endl;
    std::cout << ta.get_id() << std::endl;
    std::cout << std::hash<picoThread::thread::id>{}(ta.get_id()) << std::endl;
    ta.join();
    std::cout << "b: " << b << std::endl;
    tb.detach();
}

int main()
{
    stdio_init_all();
    sleep_ms(5000);

    createThreads();

    while (true)
    {
        std::cout << "test" << std::endl;
        sleep_ms(10000);
    }

    return 0;
}