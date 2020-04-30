#include <afina/coroutine/Engine.h>

#include <setjmp.h>
#include <stdio.h>
#include <string.h>

namespace Afina {
namespace Coroutine {

void Engine::Store(context &ctx) {
    char current_address;
    ctx.Low = StackBottom;
    ctx.Hight = &current_address;

    if (ctx.Hight < ctx.Low) {
        std::swap(ctx.Hight, ctx.Low);
    }
    auto &buf = std::get<0>(ctx.Stack);
    auto &size = std::get<1>(ctx.Stack);
    auto new_size = ctx.Hight - ctx.Low;

    if (size < new_size) {
        delete[] buf;
        buf = new char[new_size];
        size = new_size;
    }
    memcpy(buf, ctx.Low, new_size);
    ctx.Stack = std::make_tuple(buf, new_size);
}

void Engine::Restore(context &ctx) {
    char current_address;
    if (ctx.Low <= &current_address && &current_address < ctx.Hight) {
        Restore(ctx);
    }

    auto &buf = std::get<0>(ctx.Stack);
    auto size = std::get<1>(ctx.Stack);
    memcpy(ctx.Low, buf, size);
    cur_routine = &ctx;
    longjmp(ctx.Environment, 1);
}


void Engine::yield() {
    if (alive == nullptr) {
        return;
    }
    if (alive == cur_routine && alive->next == nullptr) {
        return;
    }
    
    auto routine_next = alive;
    if (routine_next == cur_routine) {
        routine_next = alive->next;
    }
    Enter(*routine_next);

}

void Engine::sched(void *routine) {
    if (routine == cur_routine) {
        return;
    } else if (routine == nullptr) {
        yield();
    }
    Enter(*(static_cast<context *>(routine)));
}

void Engine::Enter(context& ctx) {
    if (cur_routine != nullptr && cur_routine != idle_ctx) {
        if (setjmp(cur_routine->Environment) > 0) {
            return;
        }
        Store(*cur_routine);
    }
    cur_routine = &ctx;
    Restore(ctx);
}


} // namespace Coroutine
} // namespace Afina


