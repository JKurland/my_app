#pragma once

#include <thread>
#include <deque>
#include <condition_variable>
#include <mutex>
#include <atomic>
#include <future>
#include <memory>
#include <exception>
#include <type_traits>
#include <cstdint>

#include "meta.h"

namespace detail {
    class IJob {
    public:
        virtual void run() = 0;
    };

    template<class LambdaT>
    class Job: public IJob {
    public:
        Job(LambdaT lambda): lambda(std::move(lambda)) {}
        void run() override {
            lambda();
        }
    private:
        LambdaT lambda;
    };

    class Worker {
    public:
        Worker(std::size_t max_queue_size):
            buffer(),
            mutex(),
            cv(),
            stop(false),
            max_queue_size(max_queue_size),
            thread([this]{run();})
            {}

        ~Worker() {
            stop = true;
            cv.notify_one();
            thread.join();
        }

        Worker(const Worker&) = delete;
        Worker(Worker&&) = delete;

        Worker& operator=(const Worker&) = delete;
        Worker& operator=(Worker&&) = delete;

        template<class FunctionT>
        void add_job(FunctionT f) {
            {
                if (max_queue_size && buffer.size() >= max_queue_size) {
                    return;
                }
                auto job = std::make_unique<detail::Job<FunctionT>>(std::move(f));
                std::lock_guard<std::mutex> lock(mutex);
                buffer.emplace_back(std::move(job));
            }
            cv.notify_one();
        }

    private:
        std::deque<std::unique_ptr<detail::IJob>> buffer;
        std::mutex mutex;
        std::condition_variable cv;
        std::atomic<bool> stop{false};
        std::size_t max_queue_size;
        std::thread thread;

        void run() {
            while (true) {
                std::unique_lock<std::mutex> lock(mutex);
                cv.wait(lock, [&]{return stop || !buffer.empty();});

                if (stop) {
                    return;
                }

                auto job = std::move(buffer.front());
                buffer.pop_front();
                lock.unlock();
                job->run();
            }
        }
    };
}

template<typename HandlerT>
class Buffered {
public:
    Buffered(HandlerT handler, std::size_t max_queue_size=0): 
        handler(std::make_unique<HandlerT>(std::move(handler))),
        worker(std::make_unique<detail::Worker>(max_queue_size))
        {}

    template<typename CtxT, typename EventT, typename = std::enable_if_t<can_call<HandlerT, CtxT&, EventT>::value>>
    auto operator()(CtxT& ctx, EventT event) {
        std::promise<decltype((*handler)(ctx, std::move(event)))> promise;
        auto future = promise.get_future();

        worker->add_job([h=handler.get(), &ctx, e=std::move(event), p=std::move(promise)] () mutable {
            try {
                if constexpr (std::is_void_v<decltype((*h)(ctx, std::move(e)))>) {
                    (*h)(ctx, std::move(e));
                    p.set_value();
                } else {
                    p.set_value((*h)(ctx, std::move(e)));
                }
            } catch (...) {
                try {
                    p.set_exception(std::current_exception());
                } catch (...) {
                    // do nothing
                }
            }
        });
        return future;
    }
private:
    std::unique_ptr<HandlerT> handler;
    std::unique_ptr<detail::Worker> worker;
};
