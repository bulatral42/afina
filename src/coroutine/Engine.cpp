#include <afina/coroutine/Engine.h>

#include <csetjmp>
#include <cstdio>
#include <cstring>

namespace Afina {
namespace Coroutine {

void Engine::Store(context &ctx) {
    //`std::cout << "Store()" << std::endl;
    char stackEnd{};
    char *copyStart = nullptr;
    uint32_t copyLen = 0;
    ctx.StackEnd = &stackEnd;
    //std::cout << "START: " << (long unsigned int)ctx.StackStart << " " << (long unsigned int)ctx.StackEnd << std::endl;
    if (ctx.StackEnd <= ctx.StackStart) {
        //std::cout << "Q" << std::endl;
        copyStart = ctx.StackEnd;
        copyLen = ctx.StackStart - ctx.StackEnd;
    } else {
        copyStart = ctx.StackStart;
        copyLen = ctx.StackEnd - ctx.StackStart;
    }
    //std::cout << "STACK BEFORE COPY: " << (long unsigned int)(std::get<0>(ctx.Stack)) << std::endl;
    //std::cout << "STACK: " << copyStart << ", " << copyLen << std::endl;
    if (copyLen > std::get<1>(ctx.Stack) || copyLen >= 2 * std::get<1>(ctx.Stack)) {
        //std::cout << "WWWWWWW" << std::endl;
        delete[] std::get<0>(ctx.Stack);
        //std::cout << "AFTER delete" << std::endl;
        std::get<0>(ctx.Stack) = new char[copyLen];
        std::get<1>(ctx.Stack) = copyLen;
    }
    //std::cout << "STACK: " << (long unsigned int)copyStart << std::endl;
    //std::cout << copyStart[0] << /*" " << copyStart[1] << */std::endl;
    std::memcpy(std::get<0>(ctx.Stack), copyStart, copyLen);
}

void Engine::Restore(context &ctx) {
    //std::cout << "Restore()" << std::endl;
    char cur{};
    char *Low = ctx.StackEnd, *High = ctx.StackStart;
    if (Low > High) {
        Low = ctx.StackStart;
        High = ctx.StackEnd;
    }
    if (&cur >= Low && &cur <= High) {
        Restore(ctx);
    }
    cur_routine = &ctx;
    std::memcpy(Low, std::get<0>(ctx.Stack), High - Low);
    longjmp(ctx.Environment, 1);
}

void Engine::Execute(context *ctx) {
    //std::cout << "Execute()" << std::endl;
    if (ctx == nullptr) {
        //std::cout << "Execute mullptr" << std::endl;
        return;
    }
    if (cur_routine != idle_ctx) {
        //std::cout << "Execute setjmp" << std::endl;
        //std::cout << (cur_routine == nullptr) << std::endl;
        if (setjmp(cur_routine->Environment) > 0) {
            return;
        }
        Store(*cur_routine);
    }
    Restore(*ctx);
}

void Engine::yield() {
    //std::cout << "yield()" << std::endl;
    if (alive == nullptr || (alive->next == nullptr && cur_routine == alive)) {
        return;
    }
    context *next_coro{};
    if (cur_routine == alive) {
        next_coro = alive->next;
    } else {
        next_coro = alive;
    }
    Execute(next_coro);
}

void Engine::sched(void *routine) {
    //std::cout << "sched()" << std::endl;
    if (routine == nullptr) {
        yield();
    }
    context *next_coro = static_cast<context *>(routine);
    if (next_coro == cur_routine || next_coro->is_blocked) {
        //std::cout << "IN IF sched" << std::endl;
        return;
    }
    //std::cout << "sched go Execute" << std::endl;
    Execute(next_coro);
}

void Engine::block(void *coro) {
    std::cout << "block()" << std::endl;

}

void Engine::unblock(void *coro) {
    std::cout << "unblock()" << std::endl;

}


} // namespace Coroutine
} // namespace Afina
