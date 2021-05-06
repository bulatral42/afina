#include "gtest/gtest.h"

#include <functional>
#include <iostream>
#include <sstream>

#include <afina/coroutine/Engine.h>

void simple(int &a) {
    
    std::cout << "REally simpliest: " << ++a << " at " << &a << std::endl;
}

TEST(CoroutineTest, Simpliest) {
    Afina::Coroutine::Engine e;
    int a = 42;
    std::cout << "Variable a adress before engine.start: " << &a << std::endl;
    e.start(&simple, a);
    ASSERT_EQ(a, 43);
}

void _calculator_add(int &result, int left, int right) { 
    std::cout << "MAKE add" << std::endl;
    result = left + right;
    std::cout << "DONE add" << std::endl;
}

TEST(CoroutineTest, SimpleStart) {
    Afina::Coroutine::Engine engine;

    int result;
    engine.start(_calculator_add, result, 1, 2);

    ASSERT_EQ(3, result);
}


using print_func = void (*)(Afina::Coroutine::Engine &, std::stringstream &, void *&);
using f_print_func = std::function<void(Afina::Coroutine::Engine &, std::stringstream &, void *&)>;

void printa(Afina::Coroutine::Engine &pe, std::stringstream &out, void *&other) {
    out << "A1 ";
    pe.sched(other);

    out << "A2 ";
    pe.sched(other);

    out << "A3 ";
    pe.sched(other);
}

void printb(Afina::Coroutine::Engine &pe, std::stringstream &out, void *&other) {
    out << "B1 ";
    pe.sched(other);

    out << "B2 ";
    pe.sched(other);

    out << "B3 ";
}

void printc(Afina::Coroutine::Engine &pe, std::stringstream &out, void *&other) {
    out << "A1 ";
    pe.unblock(other);
    pe.block(nullptr);

    out << "A2 ";
    pe.unblock(other);
    pe.block(nullptr);

    out << "A3 ";
    pe.unblock(other);
    pe.sched(other);
}

void printd(Afina::Coroutine::Engine &pe, std::stringstream &out, void *&other) {
    out << "B1 ";
    pe.unblock(other);
    pe.block(nullptr);

    out << "B2 ";
    pe.unblock(other);
    pe.sched(other);

    out << "B3 ";
}

std::stringstream out;
void *pa = nullptr, *pb = nullptr;

void _printer(Afina::Coroutine::Engine &pe, std::string &result, print_func p1, print_func p2) {
    pa = pb = nullptr;
    out.str("");
    // Create routines, note it doens't get control yet
    pa = pe.run(p1, pe, out, pb);
    pb = pe.run(p2, pe, out, pa);

    // Pass control to first routine, it will ping pong
    // between printa/printb greedely then we will get
    // control back
    pe.sched(pa);

    out << "END";

    // done
    result = out.str();
}

TEST(CoroutineTest, PrinterSimple) {
    Afina::Coroutine::Engine engine;

    std::string result;
    engine.start(_printer, engine, result, &printa, &printb);
    ASSERT_STREQ("A1 B1 A2 B2 A3 B3 END", result.c_str());
}

TEST(CoroutineTest, PrinterBlocking) {
    Afina::Coroutine::Engine engine;

    std::string result;
    engine.start(_printer, engine, result, &printc, &printd);
    ASSERT_STREQ("A1 B1 A2 B2 A3 B3 END", result.c_str());
}

void blocking(Afina::Coroutine::Engine &pe, int &result) {
    result = 1;
    pe.block();
    result = 2;
}

TEST(CoroutineTest, LastBlock) {
    Afina::Coroutine::Engine engine;

    int result = 0;
    engine.start(blocking, engine, result);
    ASSERT_EQ(result, 1);
}

