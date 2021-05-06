#ifndef AFINA_COROUTINE_ENGINE_H
#define AFINA_COROUTINE_ENGINE_H

#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <ostream>
#include <string>
#include <tuple>

#include <csetjmp>
#include <utility>


#include <typeinfo>


namespace Afina {
namespace Coroutine {

/**
 * # Entry point of coroutine library
 * Allows to run coroutine and schedule its execution. Not threadsafe
 */
class Engine final {
public:
    using unblocker_func = std::function<void(Engine &)>;

private:
    /**
     * A single coroutine instance which could be scheduled for execution
     * should be allocated on heap
     */
    //struct context;
    struct context {
        // coroutine stack start address
        char *StackStart = nullptr;

        // coroutine stack end address
        char *StackEnd = nullptr;

        // coroutine stack copy buffer
        std::tuple<char *, uint32_t> Stack = std::make_tuple(nullptr, 0);

        // Saved coroutine context (registers)
        jmp_buf Environment;

        // To include routine in the different lists, such as "alive", "blocked", e.t.c
        context *prev = nullptr;
        context *next = nullptr;

        // Is this coroutine in blocked list
        bool is_blocked{false};

    public:
        ~context() {
            delete[] std::get<0>(Stack);
        };
    };

    /**
     * Where coroutines stack begins
     */
    char *StackBottom;

    /*
     * Current coroutine
     */
    context *cur_routine;

    /**
     * List of routines ready to be scheduled. 
     * Note that suspended routine ends up here as well
     */
    context *alive;

    /**
     * List of corountines that sleep and can't be executed
     */
    context *blocked;

    /**
     * Context to be returned finally
     */
    context *idle_ctx;

    /**
     * Call when all coroutines are blocked
     */
    unblocker_func _unblocker;

protected:
    /**
     * Save stack of the current coroutine in the given context
     */
    void Store(context &ctx);

    /**
     * Restore stack of the given context and pass control to coroutinne
     */
    void Restore(context &ctx);

    /**
     * Start executing given coroutine
     */
    void Execute(context *ctx);

    static void null_unblocker(Engine &) {}

public:
    Engine(unblocker_func unblocker = null_unblocker)
        : StackBottom(0), cur_routine(nullptr), alive(nullptr), blocked(nullptr), 
        _unblocker(unblocker) {}
    Engine(Engine &&) = delete;
    Engine(const Engine &) = delete;
    ~Engine();

    
    friend std::ostream &operator <<(std::ostream &out, const Engine &e) {
        out << "I am Engine";
        return out;
    }
    

    void *cur_coro() {
        return static_cast<void *>(cur_routine);
    }
    /**
     * Gives up current routine execution and let engine to schedule other one. 
     * It is not defined when routine will get execution back, 
     * for example if there are no other coroutines then executing could
     * be trasferred back immediately (yield turns to be noop).
     *
     * Also there are no guarantee what coroutine will get execution, 
     * it could be caller of the current one or any other which is ready to run
     */
    void yield();

    /**
     * Suspend current routine and transfers control to the given one, 
     * resumes its execution from the point when it has been suspended previously.
     *
     * If routine to pass execution to is not specified (nullptr) 
     * then method should behaves like yield. 
     * In case passed routine is the current one method does nothing
     */
    void sched(void *routine);

    /**
     * Blocks current routine so that is can't be scheduled anymore
     * If it was a currently running corountine, then do yield to select new one to be run instead.
     *
     * If argument is nullptr then block current coroutine
     */
    void block(void *coro = nullptr);

    /**
     * Put coroutine back to list of alive, so that it could be scheduled later
     */
    void unblock(void *coro);

    template<typename T>
    void unpack(T &&arg) {
        std::cout << typeid(arg).name();
        //if (typeid(arg).name() == std::string("i")) {
        //    std::cout << ": " << arg;
        //}
        std::cout << " at " << &arg << std::endl;
    }

    template<typename T, typename... TT>
    void unpack(T &&arg1, TT &&... args) {
        std::cout << typeid(arg1).name();
        //if (typeid(arg1).name() == std::string("i")) {
        //    std::cout << ": " << arg1;
        //}
        std::cout << " at " << &arg1 << ",  ";
        unpack(std::forward<TT>(args)...);
    }

    /**
     * Entry point into the engine. Prepare all internal mechanics and starts given function 
     * which is considered as main.
     *
     * Once control returns back to caller of start all coroutines are done execution, 
     * in other words, this function doesn't return control until all coroutines are done.
     *
     * @param pointer to the main coroutine
     * @param arguments to be passed to the main coroutine
     */
    template <typename... Ta> 
    void start(void (*main)(Ta...), Ta &&... args) {
        std::function<void(Ta...)> f(main);
        start(std::move(f), std::forward<Ta>(args)...);
    }
    
    template <typename... Ta> 
    void start(std::function<void(Ta...)> main, Ta &&... args) {
        // To acquire stack begin, create variable on stack and remember its address
        char StackStartsHere;
        this->StackBottom = &StackStartsHere;

        // Start routine execution
        //std::function<void(Ta...)> tmpmain(main);
        unpack(std::forward<Ta>(args)...);
        void *pc = run(main, std::forward<Ta>(args)...);
        std::cout << "pc is set to run" << std::endl;

        idle_ctx = new context();
        idle_ctx->StackStart = idle_ctx->StackEnd = &StackStartsHere;
        if (setjmp(idle_ctx->Environment) > 0) {
            if (alive == nullptr) {
                std::cout << "alive == nullptr" << std::endl;
                _unblocker(*this);
            }
            // Here: correct finish of the coroutine section
            std::cout << "idle_ctx restored" << std::endl;
            yield();
        } else if (pc != nullptr) {
            std::cout << "pc != nullptr" << std::endl;
            Store(*idle_ctx);
            std::cout << "idle_ctx is Stored " << std::endl;
            cur_routine = idle_ctx;
            sched(pc);
            std::cout << "pc is Sched" << std::endl;
        }
        std::cout << "Delete idle_ctx" << std::endl;

        // Shutdown runtime
        delete idle_ctx;
        this->StackBottom = nullptr;
    }
    
    template <typename... Ta> 
    void *run(const std::function<void(Ta...)> &func, Ta &&... args) {
        volatile char c;
        unpack(std::forward<Ta>(args)...);
        return _run((char *)&c, func, std::forward<Ta>(args)...);
    }
    template <typename... Ta> 
    void *run(void (*func)(Ta...), Ta &&... args) {
        volatile char c;
        //std::function<void(decltype(std::forward<Ta>(args)...))> f(func);
        std::function<void(Ta...)> f(func);
        unpack(f, std::forward<Ta>(args)...);
        return _run((char *)&c, f, std::forward<Ta>(args)...);
    }
    /**
     * Register new coroutine. It won't receive control until scheduled explicitely or implicitly. 
     * In case of some errors function returns -1
     */
    template <typename... Ta> 
    void *_run(char *stackStart, const std::function<void(Ta...)> &f, Ta &&... args) {
        if (this->StackBottom == 0) {
            // Engine wasn't initialized yet
            return nullptr;
        }
        std::function<void(Ta...)> func(f);

        // New coroutine context that carries around all information enough to call function
        context *pc = new context();
        pc->StackStart = pc->StackEnd = stackStart;

        // Store current state right here, i.e just before enter new coroutine, 
        // later, once it gets scheduled execution starts here. 
        // Note that we have to acquire stack of the current function call to ensure
        // that function parameters will be passed along
        printf("Stack Starts At %lx \n", (unsigned long)stackStart);
        std::cout << "Unpack before setjmp in _run:" << std::endl;
        unpack(std::forward<Ta>(args)...);
        if (setjmp(pc->Environment) > 0) {
            // Created routine got control in order to start execution. 
            // Note that all variables, such as context pointer, arguments and 
            // a pointer to the function comes from restored stack

            // invoke routine
            std::cout << "Unpack and RUN func in _run" << std::endl;
            unpack(std::forward<Ta>(args)...);
            func(std::forward<Ta>(args)...);

            // Routine has completed its execution, time to delete it. 
            // Note that we should be extremely careful in where to pass control after that. 
            // We never want to go backward by stack as that would mean to go backward in time. 
            // Function run() has already return once (when setjmp returns 0), 
            // so return second return from run would looks a bit awkward
            if (pc->prev != nullptr) {
                pc->prev->next = pc->next;
            }

            if (pc->next != nullptr) {
                pc->next->prev = pc->prev;
            }

            if (alive == cur_routine) {
                alive = alive->next;
            }

            // current coroutine finished, and the pointer is not relevant now
            cur_routine = nullptr;
            pc->prev = pc->next = nullptr;
            delete pc;

            // We cannot return here, as this function "returned" once already, 
            // so here we must select some other coroutine to run. 
            // As current coroutine is completed and can't be scheduled anymore, it is safe to
            // just give up and ask scheduler code to select someone else, 
            // control will never returns to this one
            Restore(*idle_ctx);
        }

        // setjmp remembers position from which routine could starts execution, 
        // but to make it correctly, it is neccessary to save arguments, 
        // pointer to body function, pointer to context, e.t.c - i.e. save stack.
        Store(*pc);
        // Add routine as alive double-linked list
        pc->next = alive;
        alive = pc;
        if (pc->next != nullptr) {
            pc->next->prev = pc;
        }

        return pc;
    }
};

} // namespace Coroutine
} // namespace Afina

#endif // AFINA_COROUTINE_ENGINE_H
