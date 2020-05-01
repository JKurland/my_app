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
        Worker():
            buffer(),
            mutex(),
            cv(),
            stop(false),
            thread([this]{run();})
            {}

        ~Worker() {
            stop = true;
            cv.notify_one();
            thread.join();
        }

        template<class T>
        void add_job(T t) {
            {
                std::lock_guard<std::mutex> lock(mutex);
                buffer.emplace_back(std::make_unique<detail::Job<T>>(std::move(t)));
            }
            cv.notify_one();
        }

    private:
        std::deque<std::unique_ptr<detail::IJob>> buffer;
        std::mutex mutex;
        std::condition_variable cv;
        std::atomic<bool> stop{false};
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
    Buffered(HandlerT handler): 
        handler(std::make_unique<HandlerT>(std::move(handler))),
        worker(std::make_unique<detail::Worker>())
        {}

    template<typename CtxT, typename EventT>
    auto operator()(CtxT& ctx, const EventT& event) {
        if constexpr (can_call<decltype(*handler), CtxT&, EventT&>::value) {
            std::promise<decltype((*handler)(ctx, event))> promise;
            auto future = promise.get_future();

            {
                worker->add_job([h=handler.get(), &ctx, event, p=std::move(promise)] () mutable {
                    try {
                        if constexpr (std::is_same_v<decltype((*h)(ctx, event)), void>) {
                            (*h)(ctx, event);
                            p.set_value();
                        } else {
                            p.set_value((*h)(ctx, event));
                        }
                    } catch (...) {
                        try {
                            p.set_exception(std::current_exception());
                        } catch (...) {
                            // do nothing
                        }
                    }
                });
            }
            return future;
        }
    }
private:
    std::unique_ptr<HandlerT> handler;
    std::unique_ptr<detail::Worker> worker;
};