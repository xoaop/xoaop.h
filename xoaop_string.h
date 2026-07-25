/*
    xoaop_string.h — 字符串 & 切片 (xpString, xpSlice)

    依赖: xoaop.h (需要类型定义、宏、xpAllocator)
*/

#ifndef XOAOP_STRING_H
#define XOAOP_STRING_H

#if !defined(XOAOP_H)
#error "xoaop_string.h requires xoaop.h to be included first"
#endif

#include <string>
#include <string_view>
#if __cplusplus >= 202300L
#include <format>
#endif

#if defined(__cplusplus)
extern "C" {
#endif

/*
    字符串, 切片
*/

xp_define isize xp_strlen_c(char const *str);
xp_define char *xp_find_first_non_space(const char *str);


// NOTE(xoaop): ONLY ASCII NOW
typedef struct xpString {
    xpAllocator allocator;
    isize length;
    isize capacity;
    char *c_str;

#if defined(__cplusplus)
        bool operator== (xpString other) const;
        char operator[] (isize index) const;
        char *as_c_str() const;
#endif // __cplusplus


} xpString;



typedef struct xpSlice {
    void *data;
    isize len;
} xpSlice;


xp_define xpString xp_string_c(char const *str);
xp_define xpString xp_make_string(xpAllocator allocator, char const *str);
xp_define xpString xp_make_string_capacity(xpAllocator allocator, void const *str, isize capacity);
xp_define xpString xp_make_string_count(xpAllocator allocator, char const *str, isize count);
xp_define xpString xp_make_string_zero();
xp_define xpString xp_string_to_c_style(xpString string, xpAllocator allocator);
xp_define xpString xp_make_string_from_slice(xpAllocator allocator, xpSlice slice);
xp_define xpString xp_string_copy(xpAllocator allocator, xpString string);
xp_define void xp_string_free(xpString string);
xp_define b32 xp_string_cmp(xpString a, xpString b);
xp_define b32 xp_string_equal(xpString a, xpString b);
xp_define xpString *xp_string_extend(xpString *string, isize extended_size);
xp_define isize xp_string_find_char(xpString str, char c);
xp_define xpString *xp_string_insert(isize pos, xpString *str, xpString inserted_str);
xp_define xpString *xp_string_append(xpString *str, xpString appended_str);
xp_define xpString *xp_string_append_char(xpString *str, char c);
xp_define xpString xp_isize_to_string(isize value, xpAllocator allocator);
xp_define xpString xp_string_replace_char(xpString string, char old_char, char new_char, xpAllocator allocator);



xp_define xpSlice xp_slice_make(void *data, isize len);
xp_define xpSlice xp_slice_make_from_string(xpString string, isize begin, isize len);

#if defined(__cplusplus)
}
#endif


#if defined(__cplusplus)

//
// xpString CPP 相关操作声明
//

xpString xp_string_concat_mid(xpString a, xpString b, xpOption<xpString> middle, xpAllocator allocator);


#if __cplusplus >= 202300L

template<>
struct std::formatter<xpString> : std::formatter<std::string_view> {
    using std::formatter<std::string_view>::parse;


    auto format(const xpString& s, std::format_context& ctx) const {
        return std::formatter<std::string_view>::format(
            std::string_view(s.c_str, static_cast<size_t>(s.length)),
            ctx
        );
    }
};

#endif // C++23

#endif // __cplusplus


#if defined(XOAOP_IMPLEMENTATION) && !defined(XOAOP_STRING_IMPLEMENTATION_DONE)
#define XOAOP_STRING_IMPLEMENTATION_DONE

#if defined(__cplusplus)
extern "C" {
#endif


//
// 字符串实现
//
isize xp_strlen_c(char const *str) {
    isize len = 0;
    for (; str[len] != '\0'; len += 1);

    //NOTE: for unknown wrong, maybe unnessery
    XP_ASSERT(len >= 0);
    return len;
}

void xp_strncpy_c(char *dst, const char *src, isize len) {
    if (len <= 0 || dst == NULL || src == NULL) {
        return;
    }

    isize index = 0;
    for (; index < len && src[index] != '\0'; index += 1) {
        dst[index] = src[index];
    }

    //NOTE(xoaop): 兼容c字符串
    dst[index] = '\0'; // 正确：在实际拷贝结束位置补零，而不是强制在len-1位置
}


char *xp_find_first_non_space(const char *str) {
    XP_ASSERT_DEFAULT(str != NULL);
    while (*str && isspace(cast(unsigned char) * str)) {
        str++;
    }
    return cast(char *)str;
}



xpString xp_string_c(char const *str) {
    XP_ASSERT_DEFAULT(str != NULL);

    xpString string = {
        .allocator = {NULL, NULL},
        .length = xp_strlen_c(str),
        .capacity = xp_strlen_c(str),
        .c_str = cast(char *)str
    };
    return string;
}


xpString xp_make_string(xpAllocator allocator, char const *str) {
    return xp_make_string_capacity(allocator, str, xp_strlen_c(str));
}


xpString xp_make_string_capacity(xpAllocator allocator, void const *str, isize capacity) {
    XP_ASSERT_DEFAULT(capacity >= 0);


    xpString string;
    string.allocator = allocator;
    string.c_str = cast(char *) xp_alloc(string.allocator, capacity + 1); // NOTE: +1是为了末尾的'\0'
    string.capacity = capacity;

    // 计算实际长度
    isize length = 0;
    if (str != NULL) {
        length = xp_strlen_c(cast(char const *)str);
    }
    if (length > capacity) {
        length = capacity;
    }
    string.length = length;


    // 清空空间
    memset(string.c_str, '\0', string.capacity + 1); // NOTE: +1是为了末尾的'\0'
    // 复制字符串内容
    memcpy(string.c_str, str, string.length);

    // 末尾补上'\0'
    // string.c_str[length] = '\0';
    return string;
}

xpString xp_make_string_count(xpAllocator allocator, char const *str, isize count) {
    XP_ASSERT_DEFAULT(count >= 0);

    xpString string = xp_make_string_zero();
    string.allocator = allocator;

    // 计算实际长度
    isize actual_count = 0;
    if (str != NULL) {
        actual_count = xp_strlen_c(cast(char const *)str);
    }

    isize length = 0;
    if (actual_count < count) {
        length = actual_count;
    } else {
        length = count;
    }
    string.length = length;

    string.c_str = cast(char *) xp_alloc(string.allocator, length + 1); // NOTE: +1是为了末尾的'\0'
    string.capacity = length;

    // 复制字符串内容
    memcpy(string.c_str, str, string.length);

    // 末尾补上'\0'
    string.c_str[string.capacity] = '\0';


    return string;
}

xpString xp_make_string_zero() {
    xpString string;
    string.allocator = { NULL, NULL };
    string.c_str = NULL;
    string.length = 0;
    string.capacity = 0;
    return string;
}

xpString xp_string_to_c_style(xpString string, xpAllocator allocator) {
    xpString c_style_string = xp_make_string_count(allocator, string.c_str, string.length);
    return c_style_string;
}


void xp_string_free(xpString string) {
    if (string.allocator.proc != NULL) {
        xp_free(string.allocator, string.c_str);
    }
}

b32 xp_string_cmp(xpString a, xpString b) {
    if (a.length > b.length) {
        return 1;
    } else if (a.length < b.length) {
        return -1;
    }

    for (isize i = 0; i < a.length; i++) {
        if (a.c_str[i] != b.c_str[i]) {
            return (a.c_str[i] > b.c_str[i]) ? 1 : -1;
        }
    }

    return 0;
}

b32 xp_string_equal(xpString a, xpString b) {
    return xp_string_cmp(a, b) == 0;
}

xpString xp_string_copy(xpAllocator allocator, xpString string) {
    xpString string_copy;

    string_copy.allocator = allocator;
    string_copy.length = string.length;
    string_copy.capacity = string.length;

    string_copy.c_str = cast(char *) xp_alloc(string_copy.allocator, string_copy.capacity + 1);
    memcpy(string_copy.c_str, string.c_str, string.length);

    string_copy.c_str[string.length] = '\0'; // 末尾补上'\0'
    return string_copy;
}


xpString *xp_string_extend(xpString *string, isize extended_size) {
    if (extended_size <= 0) {
        return string;
    }

    char *new_c_str = cast(char *)xp_alloc(
        string->allocator,
        string->capacity + extended_size + 1 // +1是为了末尾的'\0'
    );
    memcpy(new_c_str, string->c_str, string->length);
    xp_free(string->allocator, string->c_str);

    string->c_str = new_c_str;

    string->capacity = string->capacity + extended_size;

    string->c_str[string->length] = '\0'; // 末尾补上'\0'


    return string;
}

isize xp_string_find_char(xpString str, char c) {
    for (isize i = 0; i < str.length; i++) {
        if (str.c_str[i] == c) {
            return i;
        }
    }
    return -1;
}

xpString *xp_string_insert(isize pos, xpString *str, xpString inserted_str) {
    XP_ASSERT_DEFAULT(pos >= 0);
    XP_ASSERT_DEFAULT(pos <= str->length);

    // 1. 扩容，确保插入后长度不会超过 capacity
    if (str->length + inserted_str.length > str->capacity) {
        xp_string_extend(str, inserted_str.length + (str->length + inserted_str.length - str->capacity));
    }

    XP_ASSERT_DEFAULT(str->length + inserted_str.length <= str->capacity);

    // 2. 后移原有数据
    if (pos < str->length) {
        isize moved_size = str->length - pos;
        memmove(str->c_str + pos + inserted_str.length, str->c_str + pos, moved_size);
    }

    // 3. 插入新字符串
    memcpy(str->c_str + pos, inserted_str.c_str, inserted_str.length);

    str->length += inserted_str.length;

    XP_ASSERT_DEFAULT(str->length <= str->capacity);

    // 4. 末尾补零
    str->c_str[str->length] = '\0';


    return str;
}

xpString *xp_string_append_char(xpString *str, char c) {
    xpString char_str = xp_make_string_zero();
    char_str.c_str = &c;
    char_str.length = 1;
    char_str.capacity = 1;

    return xp_string_append(str, char_str);
}

xpString *xp_string_append(xpString *str, xpString appended_str) {
    return xp_string_insert(str->length, str, appended_str);
}



xpString xp_isize_to_string(isize value, xpAllocator allocator) {
    isize len = snprintf(NULL, 0, "%lld", value);

    xpString string = xp_make_string_capacity(allocator, NULL, len);
    snprintf(string.c_str, string.capacity + 1, "%lld", value);

    string.length = len;

    return string;
}

xpString xp_string_replace_char(xpString string, char old_char, char new_char, xpAllocator allocator) {
    xpString new_string = xp_string_copy(allocator, string);

    for (isize i = 0; i < new_string.length; i++) {
        if (new_string.c_str[i] == old_char) {
            new_string.c_str[i] = new_char;
        }
    }

    return new_string;
}


xpString xp_make_string_from_slice(xpAllocator allocator, xpSlice slice) {
    return xp_make_string_capacity(allocator, slice.data, slice.len);
}


xpSlice xp_slice_make(void *data, isize len) {
    xpSlice slice = {
        .data = data,
        .len = len
    };
    return slice;
}

xpSlice xp_slice_make_from_string(xpString string, isize begin, isize len) {
    if ((len < 0 || begin < 0) || ((begin + len) > string.length)) {
        XP_ASSERT(0);
    }

    xpSlice slice = {};
    slice.data = string.c_str + begin;
    slice.len = len;

    return slice;
}


#if defined(__cplusplus)
}
#endif


#if defined(__cplusplus)

//
// xpString CPP 部分实现
//

bool xpString::operator== (xpString other) const {
    b32 xp_string_cmp(xpString a, xpString b);
    return !xp_string_cmp(*this, other);
}

char xpString::operator[] (isize index) const {
    XP_ASSERT_DEFAULT(index >= 0 && index < length);
    return c_str[index];
}

char *xpString::as_c_str() const {
    // 目前都是c风格
    return c_str;
}



xpString xp_string_concat_mid(xpString a, xpString b, xpOption<xpString> middle, xpAllocator allocator) {
    xpString result = xp_string_copy(allocator, a);

    if (middle.has_value()) {
        xp_string_append(&result, middle.unwrap());
    }

    xp_string_append(&result, b);

    return result;
}

#endif // __cplusplus

#endif // XOAOP_IMPLEMENTATION

#endif // XOAOP_STRING_H
