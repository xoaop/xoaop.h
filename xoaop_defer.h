/*
    xoaop_defer.h — Defer 机制 (C++17+)

    依赖: xoaop.h
*/

#ifndef XOAOP_DEFER_H
#define XOAOP_DEFER_H

#if !defined(XOAOP_H)
#error "xoaop_defer.h requires xoaop.h to be included first"
#endif

#if defined(__cplusplus) && __cplusplus >= 201703L

template<typename F>
struct xpDeferWrapper {

    xpDeferWrapper(F defer_func) : defer_func(defer_func) {

    }
    ~xpDeferWrapper() {
        defer_func();
    }
    F defer_func;
};

#define DEFER_1(x, y) x##y
#define DEFER_2(x, y) DEFER_1(x, y)
#define DEFER_3(x) DEFER_2(x, __COUNTER__)
#define defer(code) auto DEFER_3(__deFer__) = xpDeferWrapper([&]() { code; })

#endif // __cplusplus >= 201703L

#endif // XOAOP_DEFER_H
