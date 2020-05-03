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

    template<class EventT>
    void handle_event(EventT&& t) {
        event_handler(*this, std::forward<EventT>(t));
    }

    template<class RequestT>
    auto handle_request(RequestT&& t) {
        return request_handler(*this, std::forward<RequestT>(t));
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

struct MyRequest {
    MyRequest(const MyRequest&) = delete;
    MyRequest& operator=(const MyRequest&) = delete;

    MyRequest(MyRequest&&) = default;
    MyRequest& operator=(MyRequest&&) = default;
};


int main() {

    Ctx ctx {
        Serial {
            [](auto& ctx, const auto& event) {std::cout << "Got an event with address: " << &event << std::endl;},
            // The MustHandle wrapper ensures that every event is handled by at least one handler within MustHandle.
            // If we didn't do this the log handler above might hide an error where an event isn't being handled.
            MustHandle { Serial {
                [](auto& ctx, int i){std::cout << ctx.handle_request(i).value() << std::endl;},
                [](auto& ctx, const char* i){std::cout << "c string " << i << std::endl;},
                [](auto& ctx, std::string i){std::cout << "c++ string " << i << std::endl;},

                // Because this handler only wants a const MyEvent& and it happens before
                // the owning handler this will compile.
                [](auto& ctx, const MyEvent& e){std::cout << "Inspecting MyEvent" << std::endl;},
                // Because this is the only handler that wants ownership of MyEvent, MyEvent does
                // not have to be copy constructable. It can be moved into the handler
                [](auto& ctx, MyEvent e){std::cout << "Moved from MyEvent" << std::endl;},

                // Compiler error because MyEvent can't be copied but 2 handlers want ownership
                // [](auto& ctx, MyEvent e){std::cout << "MyEvent is already taken" << std::endl;},

                Buffered {
                    Serial {
                        // [](auto& ctx, const MyEvent& e){std::cout << "Buffered can't move from MyEvent because it has already been moved from" << std::endl;},
                        // Also a compiler error, even though this handler doesn't want ownership of MyEvent
                        // it is inside a Buffered wrapper and Buffered always needs ownership to
                        // make sure the event stays alive long enough. Also a compiler error because the owning
                        // handler for MyEvent comes before this handler, so the MyEvent object
                        // will already have been moved out of.

                        // Buffered will have made a copy of the string event and move it into the Serial.
                        // Because this is the last handler in the Serial that string will be moved into this handler.
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

            // The first handler returns void, it doesn't need to know the correct type to return.
            [](auto& ctx, const char* c_str){return;},
            // The next handler returns an empty optional, this means it wasn't able
            // to answer the request, maybe a setting wasn't been provided or a dll wasn't found. 
            [](auto& ctx, const char* c_str){return std::optional<int>{};},
            // Because the previous handler wasn't able to answer, this handler is called
            [](auto& ctx, const char* c_str){return std::optional<int>(2);},
            // Because the previous handler was able to answer, this handler is not called, but 
            // it still has to return the same type or void.
            [](auto& ctx, const char* c_str){return std::optional<int>(4);},
            [](auto& ctx, const char* c_str){return;},

            // This is ok even though MyRequest is not copyable. This is because
            // the first handler doesn't return a std::optional, this indicates to
            // the compiler that the remaining request handlers never need to be called
            // so MyRequest can be forwarded into the first handler and the second handler
            // is ignored.
            [](auto& ctx, MyRequest req){return "The handler that gets called";},
            // This handler is never called because the handler above doesn't return an optional
            [](auto& ctx, MyRequest req){return "Doesn't get called because something else has non optionally answered MyRequest";},
        }
    };
    

    std::cout << ctx.handle_request(5).value() << std::endl;
    std::cout << ctx.handle_request(MyRequest{}) << std::endl;

    ctx.handle_event(MyEvent{});
    ctx.handle_event(std::string("hello"));
    ctx.handle_event(2);

    // compiler error because nothing handles a request of type string
    //ctx.handle_request(std::string("anyone there?"));
}