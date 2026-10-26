/*
    xoaop_iter.h — 三种循环形态 (C++17+)

    FOR(container) { ... }        只读遍历
        it       = 当前元素（const 引用，不能改）
        it_index = 当前元素的下标（0 开始，isize）

    FOR_MUT(container) { ... }    可写遍历
        it       = 当前元素的引用（能改，改的就是容器里的元素）
        it_index = 同上

    FOR_0_TO(n) { ... }           计次循环（没有容器可遍历时用）
        it       = 0..n-1

    三者展开都是普通 for：只用 std::begin/std::end 和一次自增计数器，
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

// 计数器 + 迭代器 + 元素的公共载体。
// R 是引用类型时（左值容器）rng 就是引用，不拷贝；
// R 是值类型时（临时容器）rng 按值持有，生命周期跟着循环走。
template <typename R>
struct xpForRange {
    R rng;
    decltype(std::begin(std::declval<R &>())) cur;
    decltype(std::end(std::declval<R &>()))   last;
    isize idx = 0;

    explicit xpForRange(R &&r) : rng(static_cast<R &&>(r)), cur(std::begin(rng)), last(std::end(rng)) {}
};

template <typename R>
xpForRange<R> xp_for_range(R &&r) {
    return xpForRange<R>(static_cast<R &&>(r));
}

// 只读遍历：it 是 const 引用，改元素会编译失败。
#define FOR(...)                                                                    \
    for (auto _for_s = xp_for_range(__VA_ARGS__);                                   \
         _for_s.cur != _for_s.last; ++_for_s.cur, ++_for_s.idx)                     \
        if ([[maybe_unused]] const isize it_index = _for_s.idx; true)               \
            if ([[maybe_unused]] const auto& it = *_for_s.cur; true)

// 可写遍历：it 是引用，赋值即改容器。数组、vector、map、临时容器、
// 任何有 begin()/end() 的类型都能用；vector<bool> 这类代理容器也行。
#define FOR_MUT(...)                                                                \
    for (auto _for_s = xp_for_range(__VA_ARGS__);                                   \
         _for_s.cur != _for_s.last; ++_for_s.cur, ++_for_s.idx)                     \
        if ([[maybe_unused]] const isize it_index = _for_s.idx; true)               \
            if ([[maybe_unused]] auto&& it = *_for_s.cur; true)

// 计次循环：重复 n 次，it 就是 0..n-1。
// 展开是单条 for，所以没有上面两个的 else 归属问题；
// 边界表达式只求值一次，n <= 0 时一次都不执行。
#define FOR_0_TO(...)                                                               \
    for ([[maybe_unused]] isize it = 0, _for_end = (__VA_ARGS__); it < _for_end; ++it)

// 两个已知边界（FOR / FOR_MUT 共用）：
//   1. 嵌套时内层 it / it_index 会遮蔽外层（-Wshadow 默认不在 -Wall 里），
//      要外层元素就先存下来：FOR(a) { auto outer = it; FOR_MUT(b) { ... } }
//   2. 展开是 if + for + if。把它当作 if 的分支时，花括号要加在**分支**上：
//          if (c) { FOR(v) f(it); } else g();   // OK
//          if (c) FOR(v) f(it); else g();       // else 会挂到宏内层那个 if
//      宏自己加花括号解决不了（else 永远绑定最近的那个未配对 if），
//      只能靠调用点把分支包起来 —— 这也是所有"展开里带 if"的宏的通病。
//   3. 依赖 begin()/end() 是同一种类型（标准容器都是），
//      带 sentinel 的 C++20 view 不适用，那类场景直接写 range-for。

#endif // __cplusplus >= 201703L

#endif // XOAOP_ITER_H
