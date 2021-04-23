#include <afina/coroutine/Engine.h>

#include <csetjmp>
#include <cstdio>
#include <cstring>

namespace Afina {
namespace Coroutine {


Engine::~Engine() {
    for (context *coro = alive; coro != nullptr;) {
        context *tmp = coro;
        coro = coro->next;
        delete[] std::get<0>(tmp->Stack);
        delete[] tmp;
    }
    for (context *coro = blocked; coro != nullptr;) {
        context *tmp = coro;
        coro = coro->next;
        delete[] std::get<0>(tmp->Stack);
        delete tmp;
    }
}

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

void Engine::block(void *routine) {
    //std::cout << "block()" << std::endl;
    context *coro = static_cast<context *>(routine);
    //std::cout << "CASTED" << std::endl;
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
    //std::cout << "DELETED from alive" << std::endl;
    coro->prev = nullptr;
    coro->next = blocked;
    //std::cout << "ENTER alive" << std::endl;
    if (blocked) {
        //std::cout << "BLOCKED = " << blocked << std::endl;
        blocked->prev = coro;
        //std::cout << "ENTER alive" << std::endl;
    }
    //std::cout << "ENTER alive" << std::endl;

    blocked = coro;
    //std::cout << "ENTER alive" << std::endl;

    coro->is_blocked = true;

    if (coro == cur_routine) {
        //std::cout << "go yield" << std::endl;
        yield();
    }
}

void Engine::unblock(void *routine) {
    //std::cout << "unblock()" << std::endl;
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
