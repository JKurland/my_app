#include <iostream>
#include <optional>
#include <string>
#include "serial.h"
#include "first.h"

template<typename EventHandlerT, typename RequestHandlerT>
class Ctx {
public:
    Ctx(EventHandlerT event_handler, RequestHandlerT request_handler): event_handler(event_handler), request_handler(request_handler){}

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


int main() {

    Ctx ctx {
        Serial {
            [](auto ctx, int i){std::cout << ctx.handle_request(i).value() << std::endl;},
            [](auto ctx, const char* i){std::cout << "c string " << i << std::endl;},
            [](auto ctx, std::string i){std::cout << "c++ string " << i << std::endl;},
        },

        First {
            [](auto ctx, int j){return ctx.handle_request("hello");},
            [](auto ctx, const char* c_str){return std::optional<int>(2);},
        }
    };
    

    std::cout << ctx.handle_request(4).value() << std::endl;
    ctx.handle_event(std::string("hello"));
    ctx.handle_event(2);
}