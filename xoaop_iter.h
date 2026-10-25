/*
    xoaop_iter.h — FOR 遍历宏 (C++17+)

    FOR(container) { ... }
        循环体里 it       = 当前元素的引用
                   it_index = 当前元素的下标（0 开始，isize）

    展开是一句普通 for：只用 std::begin/std::end 和一次自增计数器，
    不依赖 <ranges>/std::views::enumerate。

    依赖: xoaop.h
*/

#ifndef XOAOP_ITER_H
#define XOAOP_ITER_H

#if !defined(XOAOP_H)
#error "xoaop_iter.h requires xoaop.h to be included first"
#endif

#if defined(__cplusplus) && __cplusplus >= 201703L

#include <iterator> // std::begin / std::end

// 没有 lambda、没有函数调用，所以：
//   - 容器表达式只求值一次（临时对象的生命周期覆盖整个循环）；
//   - break / continue / return 语义与原样写 for 一致；
//   - 数组、std::vector、std::map、临时容器、任何有 begin()/end() 的类型都能用；
//   - 优化后与裸下标循环等价（计数器不会留在热循环里）。
//
// 两个已知边界：
//   1. 嵌套时内层 it / it_index 会遮蔽外层（-Wshadow 默认不在 -Wall 里），
//      要外层元素就先存下来：FOR(a) { auto outer = it; FOR(b) { ... } }
//   2. 展开是 if + for + if。把它当作 if 的分支时，花括号要加在**分支**上：
//          if (c) { FOR(v) f(it); } else g();   // OK
//          if (c) FOR(v) f(it); else g();       // else 会挂到宏内层那个 if
//      宏自己加花括号解决不了（else 永远绑定最近的那个未配对 if），
//      只能靠调用点把分支包起来 —— 这也是所有"展开里带 if"的宏的通病。
//      需要绝对安全时用 range-for 直接写：for (auto&& [i, v] : ...)
//   3. 依赖 begin()/end() 是同一种类型（标准容器都是），
//      带 sentinel 的 C++20 view 不适用，那类场景直接用 range-for。
#define FOR(...)                                                                    \
    if (auto&& _for_r = (__VA_ARGS__); true)                                        \
        for (struct {                                                               \
                 decltype(std::begin(_for_r)) cur;                                  \
                 decltype(std::end(_for_r))   last;                                 \
                 isize idx;                                                         \
             } _for_it{std::begin(_for_r), std::end(_for_r), 0};                    \
             _for_it.cur != _for_it.last; ++_for_it.cur, ++_for_it.idx)             \
            if ([[maybe_unused]] const isize it_index = _for_it.idx; true)          \
                if ([[maybe_unused]] auto&& it = *_for_it.cur; true)

#endif // __cplusplus >= 201703L

#endif // XOAOP_ITER_H
