/*
    xoaop_pair.h — Pair<A, B>

    依赖: xoaop.h
*/

#ifndef XOAOP_PAIR_H
#define XOAOP_PAIR_H

#if !defined(XOAOP_H)
#error "xoaop_pair.h requires xoaop.h to be included first"
#endif

#if defined(__cplusplus)

template<typename A, typename B>
struct xpPair {
    A first;
    B second;
};

template<typename A, typename B>
xpPair<A, B> xp_make_pair(A first, B second) {
    xpPair<A, B> pair = {};
    pair.first = first;
    pair.second = second;
    return pair;
}

#endif // __cplusplus

#endif // XOAOP_PAIR_H
