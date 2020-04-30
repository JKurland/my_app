#pragma once

template<typename HandlerT>
class Buffered {
public:
    Buffered(HandlerT handler): handler(handler) {}

    template<typename CtxT, typename EventT>
    void operator()
private:
    HandlerT handler;
};
