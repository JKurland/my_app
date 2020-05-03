#include <iostream>
#include <optional>
#include <string>
#include "event/serial.h"
#include "event/first.h"
#include "event/buffered.h"
#include "event/must_handle.h"

template<typename EventHandlerT, typename RequestHandlerT>
class Ctx {
public:
    Ctx(EventHandlerT event_handler, RequestHandlerT request_handler):
        event_handler(std::move(event_handler)),
        request_handler(std::move(request_handler)){}

    template<class T>
    void handle_event(T&& t) {
        event_handler(*this, std::forward<T>(t));
    }

    template<class T>
    auto handle_request(T&& t) {
        return request_handler(*this, std::forward<T>(t));
    }
private:
    EventHandlerT event_handler;
    RequestHandlerT request_handler;
};


struct MyEvent {
    MyEvent(const MyEvent&) = delete;
    MyEvent& operator=(const MyEvent&) = delete;

    MyEvent(MyEvent&&) = default;
    MyEvent& operator=(MyEvent&&) = default;
};

int main() {

    Ctx ctx {
        Serial {
            [](auto& ctx, const auto& event) {std::cout << "Got an event with address: " << &event << std::endl;},
            MustHandle { Serial {
                [](auto& ctx, int i){std::cout << ctx.handle_request(i).value() << std::endl;},
                [](auto& ctx, const char* i){std::cout << "c string " << i << std::endl;},
                [](auto& ctx, std::string i){std::cout << "c++ string " << i << std::endl;},

                // Because this is the only handler that wants ownership of MyEvent, MyEvent does
                // not have to be copy constructable. It can be moved into the handler
                [](auto& ctx, MyEvent e){std::cout << "Got MyEvent" << std::endl;},
                Buffered {
                    Serial {
                        // Compiler error because MyEvent can't be copied but 2 handlers want ownership
                        // [](auto& ctx, MyEvent e){std::cout << "Also Got MyEvent" << std::endl;},

                        // Also a compiler error, even though this handler doesn't want ownership of MyEvent
                        // it is inside a Buffered wrapper and Buffered always needs ownership to
                        // make sure the event stay alive long enough. Also a compiler error because the owning
                        // handler for MyEvent comes before this handler, so the MyEvent object
                        // will already have been moved out of.
                        // [](auto& ctx, const MyEvent& e){std::cout << "Also Got MyEvent" << std::endl;},

                        [](auto& ctx, std::string i){std::cout << "A c++ string " << i << std::endl;},
                    }
                },
                Buffered {
                    [](auto& ctx, std::string i){std::cout << "B c++ string " << i << std::endl;},
                },
            }},
        },


        First {
            [](auto& ctx, int j){return ctx.handle_request("hello");},
            [](auto& ctx, const char* c_str){return std::optional<int>(2);},
        }
    };
    

    // std::cout << ctx.handle_request(4).value() << std::endl;
    ctx.handle_event(MyEvent{});
    ctx.handle_event(std::string("hello"));
    ctx.handle_event(2);

    // compiler error because nothing handles a request of type string
    //ctx.handle_request(std::string("anyone there?"));
}