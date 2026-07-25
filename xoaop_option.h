/*
    xoaop_option.h — Option<T>

    依赖: xoaop.h
*/

#ifndef XOAOP_OPTION_H
#define XOAOP_OPTION_H

#if !defined(XOAOP_H)
#error "xoaop_option.h requires xoaop.h to be included first"
#endif

#if defined(__cplusplus)

enum class xpOptionEnum {
    None,
    Some
};

template<typename T>
struct xpOption {
public:

    static xpOption<T> none() {
        return xpOption();
    }

    static xpOption<T> some(T value) {
        return xpOption(value);
    }

    xpOption() : kind(xpOptionEnum::None) {}
    xpOption(T val) : kind(xpOptionEnum::Some), value(val) {}

    bool has_value() {
        return kind == xpOptionEnum::Some;
    }

    bool is_none() {
        return kind == xpOptionEnum::None;
    }

    T unwrap() {
        XP_ASSERT_DEFAULT(kind == xpOptionEnum::Some);
        return value;
    }


    bool operator==(const xpOption<T> &other) const {
        if (kind != other.kind) {
            return false;
        }
        if (kind == xpOptionEnum::None) {
            return true; // 两个都是None
        }
        return value == other.value; // 比较Some的值
    }


private:
    xpOptionEnum kind;
    T value;
};

#endif // __cplusplus

#endif // XOAOP_OPTION_H
