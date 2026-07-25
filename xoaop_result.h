/*
    xoaop_result.h — Result<Ok, Err> + match()

    依赖: xoaop.h
*/

#ifndef XOAOP_RESULT_H
#define XOAOP_RESULT_H

#if !defined(XOAOP_H)
#error "xoaop_result.h requires xoaop.h to be included first"
#endif

#include <concepts>

#if defined(__cplusplus)

template<typename F, typename I>
concept MatchHandler = requires(F handler, I && input) {
    { handler(input) } -> std::same_as<void>;
};


enum class xpResultType {
    Ok,
    Err
};


template<typename OkType, typename ErrType>
struct xpResult {
    static_assert(!std::is_same_v<OkType, ErrType>, "OkType and ErrType must be different types");


    static xpResult<OkType, ErrType> ok(OkType value) {
        return xpResult(value);
    }

    static xpResult<OkType, ErrType> err(ErrType error) {
        return xpResult(error);
    }

    bool is_ok() const {
        return type == xpResultType::Ok;
    }

    bool is_err() const {
        return type == xpResultType::Err;
    }

    OkType as_ok() const {
        XP_ASSERT_DEFAULT(is_ok());
        return ok_val;
    }

    ErrType as_err() const {
        XP_ASSERT_DEFAULT(is_err());
        return err_val;
    }


private:

    xpResult(OkType ok_value) : type(xpResultType::Ok), ok_val(ok_value) {}
    xpResult(ErrType err_value) : type(xpResultType::Err), err_val(err_value) {}

    xpResultType type;
    union {
        OkType ok_val;
        ErrType err_val;
    };
};

template<typename OkType, typename ErrType>
void match(xpResult<OkType, ErrType> &result, MatchHandler<OkType> auto &&ok_handler, MatchHandler<ErrType> auto &&err_handler) {
    if (result.is_ok()) {
        ok_handler(result.as_ok());
    } else if (result.is_err()) {
        err_handler(result.as_err());
    } else {
        UNREACHABLE();
    }
}

#endif // __cplusplus

#endif // XOAOP_RESULT_H
