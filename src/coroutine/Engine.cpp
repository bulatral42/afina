#include <afina/coroutine/Engine.h>

#include <csetjmp>
#include <cstdio>
#include <cstring>
#include <cassert>

namespace Afina {
namespace Coroutine {


Engine::~Engine() {
    for (context *coro = alive; coro != nullptr;) {
        context *tmp = coro;
        coro = coro->next;
        delete[] tmp;
    }
    for (context *coro = blocked; coro != nullptr;) {
        context *tmp = coro;
        coro = coro->next;
        delete tmp;
    }
}

void Engine::Store(context &ctx) {
    char stackEnd{};
    char *newStackStart = nullptr;
    uint32_t newStackLen = 0;
    ctx.StackEnd = &stackEnd;
    if (ctx.StackEnd <= ctx.StackStart) {
        newStackStart = ctx.StackEnd;
        newStackLen = ctx.StackStart - ctx.StackEnd;
    } else {
        newStackStart = ctx.StackStart;
        newStackLen = ctx.StackEnd - ctx.StackStart;
    }
    if (newStackLen > std::get<1>(ctx.Stack) || 2 * newStackLen <= std::get<1>(ctx.Stack)) {
        delete[] std::get<0>(ctx.Stack);
        std::get<0>(ctx.Stack) = new char[newStackLen];
        std::get<1>(ctx.Stack) = newStackLen;
    }
    printf("STORED from %lx to %lx", (unsigned long)ctx.StackStart, (unsigned long)ctx.StackEnd);
    std::cout << std::endl;
    std::memcpy(std::get<0>(ctx.Stack), newStackStart, newStackLen);
}

void Engine::Restore(context &ctx) {
    char cur{};
    char *Low = ctx.StackEnd, *High = ctx.StackStart;
    if (Low > High) {
        Low = ctx.StackStart;
        High = ctx.StackEnd;
    }
    if (&cur >= Low && &cur <= High) {
        std::cout << "SKIP" << std::endl;
        Restore(ctx);
    }
    cur_routine = &ctx;
    std::memcpy(Low, std::get<0>(ctx.Stack), High - Low);
    std::cout << "Before Longjmp" << std::endl;
    longjmp(ctx.Environment, 1);
}

void Engine::Execute(context *ctx) {
    assert(ctx != cur_routine);
    if (ctx == nullptr) {
        return;
    }
    if (cur_routine != idle_ctx) {
        std::cout << "before setjmp" << std::endl;
        if (setjmp(cur_routine->Environment) > 0) {
            std::cout << "setjmp > 0" << std::endl;
            return;
        }
        Store(*cur_routine);
    }
    std::cout << "before restore" << std::endl;
    Restore(*ctx);
}

void Engine::yield() {
    if (alive == nullptr || (alive->next == nullptr && cur_routine == alive)) {
        return;
    }
    context *next_coro{};

    if (cur_routine == alive) {
        next_coro = alive->next;
    } else {
        std::cout << "take alive from head in yield" << std::endl;
        next_coro = alive;
    }
    Execute(next_coro);
}

void Engine::sched(void *routine) {
    if (routine == nullptr) {
        yield();
    }
    context *next_coro = static_cast<context *>(routine);
    if (next_coro == cur_routine || next_coro->is_blocked) {
        return;
    }
    std::cout << "Start Executing" << std::endl;
    Execute(next_coro);
}

void Engine::block(void *routine) {
    context *coro = static_cast<context *>(routine);
    if (routine == nullptr) {
        coro = cur_routine;
    }
    if (coro == nullptr || coro->is_blocked) {
        return;
    }
    if (coro->prev) {
        coro->prev->next = coro->next;
    }
    if (coro->next) {
        coro->next->prev = coro->prev;
    }
    if (coro == alive) {
        alive = alive->next;
    }
    coro->prev = nullptr;
    coro->next = blocked;
    if (blocked) {
        blocked->prev = coro;
    }
    
    blocked = coro;

    coro->is_blocked = true;

    if (coro == cur_routine) {
        std::cout << "block(cur_routine)" << std::endl;
        Execute(idle_ctx);
    }
}

void Engine::unblock(void *routine) {
    context *coro = static_cast<context *>(routine);
    if (coro == nullptr || !coro->is_blocked) {
        return;
    }
    if (coro->prev) {
        coro->prev->next = coro->next;
    }
    if (coro->next) {
        coro->next->prev = coro->prev;
    }
    if (coro == blocked) {
        blocked = blocked->next;
    }

    coro->prev = nullptr;
    coro->next = alive;
    if (alive) {
        alive->prev = coro;
    }
    alive = coro;

    coro->is_blocked = false;
}


} // namespace Coroutine
} // namespace Afina
