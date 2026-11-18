/*
    xoaop_string.h — 字符串 & 切片 (xpString, xpSlice)

    依赖: xoaop.h (需要类型定义、宏、xpAllocator)
*/

#ifndef XOAOP_STRING_H
#define XOAOP_STRING_H

#if !defined(XOAOP_H)
#error "xoaop_string.h requires xoaop.h to be included first"
#endif

#if defined(__cplusplus)
#include <string>
#include <string_view>
#if __cplusplus >= 202300L
#include <format>
#endif
#endif

#if defined(__cplusplus)
extern "C" {
#endif

/*
    字符串, 切片
*/

xp_define char *xp_find_first_non_space(const char *str);


// NOTE(xoaop): ONLY ASCII NOW
// NOTE(xoaop): 元数据在字符数据前面, c_str 指向字符数据
//   [ xpStringMetaData | char data | '\0' ]
//   ^                   ^ c_str
typedef struct xpStringMetaData {
    xpAllocator allocator;
    isize count;
    isize capacity;
} xpStringMetaData;

#if defined(__cplusplus)
#define XP_STRING_META(str) (((xpStringMetaData *)(cast(char *)(str))) - 1)
#else
#define XP_STRING_META(str) (((xpStringMetaData *)((str).c_str)) - 1)
#endif

typedef struct xpString {
#if defined(__cplusplus)
private:
#endif
    char *c_str;

#if defined(__cplusplus)
public:
    xpString();
    xpString(char const *str);
    xpString(xpAllocator allocator, char const *str);
    operator char *() const;
    bool operator==(xpString other) const;
    bool operator!=(xpString other) const;
    bool operator==(char const *other) const;
    bool operator!=(char const *other) const;

    friend xpString xp_string_alloc(xpAllocator allocator, isize capacity);
#endif // __cplusplus
} xpString;


typedef struct xpSlice {
    void *data;
    isize len;
} xpSlice;


xp_define isize xp_string_count(xpString string);
xp_define isize xp_string_capacity(xpString string);
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
xp_define xpString *xp_string_append_c(xpString *str, char const *appended_str);
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

// NOTE(xoaop): const char* 重载, 让字面量直接参与比较/追加
inline b32 xp_string_cmp(xpString a, char const *b) {
    isize a_count = xp_string_count(a);
    isize b_count = b != NULL ? cast(isize)strlen(b) : 0;
    if (a_count > b_count) {
        return 1;
    } else if (a_count < b_count) {
        return -1;
    }

    if (a_count == 0) {
        return 0;
    }
    return memcmp(a, b, static_cast<size_t>(a_count));
}

inline b32 xp_string_equal(xpString a, char const *b) {
    return xp_string_cmp(a, b) == 0;
}

inline isize xp_string_find_char(char const *str, char c) {
    if (str == NULL) {
        return -1;
    }
    for (isize i = 0; str[i] != '\0'; i++) {
        if (str[i] == c) {
            return i;
        }
    }
    return -1;
}

inline xpString *xp_string_append(xpString *str, char const *appended_str) {
    return xp_string_append_c(str, appended_str);
}

inline xpString xp_string_replace_char(char const *string, char old_char, char new_char, xpAllocator allocator) {
    xpString new_string = xp_make_string(allocator, string);

    isize count = xp_string_count(new_string);
    for (isize i = 0; i < count; i++) {
        if (new_string[i] == old_char) {
            new_string[i] = new_char;
        }
    }

    return new_string;
}

inline xpSlice xp_slice_make_from_string(char const *string, isize begin, isize len) {
    isize string_count = string != NULL ? cast(isize)strlen(string) : 0;
    if ((len < 0 || begin < 0) || ((begin + len) > string_count)) {
        XP_ASSERT(0);
    }

    xpSlice slice = {};
    slice.data = cast(void *)(string + begin);
    slice.len = len;

    return slice;
}

xpString xp_string_concat_mid(xpString a, xpString b, xpOption<xpString> middle, xpAllocator allocator);


#if __cplusplus >= 202300L

template<>
struct std::formatter<xpString> : std::formatter<std::string_view> {
    using std::formatter<std::string_view>::parse;


    auto format(const xpString& s, std::format_context& ctx) const {
        const char *data = cast(char *)(s) != NULL ? cast(char *)(s) : "";
        std::string_view view(data, static_cast<size_t>(xp_string_count(s)));
        return std::formatter<std::string_view>::format(view, ctx);
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
char *xp_find_first_non_space(const char *str) {
    XP_ASSERT_DEFAULT(str != NULL);
    while (*str && isspace(cast(unsigned char) * str)) {
        str++;
    }
    return cast(char *)str;
}


xpString xp_string_alloc(xpAllocator allocator, isize capacity) {
    XP_ASSERT_DEFAULT(capacity >= 0);

    isize size = cast(isize)sizeof(xpStringMetaData) + capacity + 1;
    char *base = cast(char *)xp_alloc(allocator, size);
    XP_ASSERT_DEFAULT(base != NULL);
    if (base == NULL) {
        return xp_make_string_zero();
    }
    memset(base, 0, cast(size_t)size);

    xpStringMetaData *meta = cast(xpStringMetaData *)base;
    meta->allocator = allocator;
    meta->count = 0;
    meta->capacity = capacity;

    xpString string;
    string.c_str = base + sizeof(xpStringMetaData);
    return string;
}

isize xp_string_count(xpString string) {
    if (cast(char *)(string) == NULL) {
        return 0;
    }
    return XP_STRING_META(string)->count;
}

isize xp_string_capacity(xpString string) {
    if (cast(char *)(string) == NULL) {
        return 0;
    }
    return XP_STRING_META(string)->capacity;
}


xpString xp_make_string(xpAllocator allocator, char const *str) {
    if (str == NULL) {
        return xp_make_string_zero();
    }
    return xp_make_string_capacity(allocator, str, cast(isize)strlen(str));
}


xpString xp_make_string_capacity(xpAllocator allocator, void const *str, isize capacity) {
    XP_ASSERT_DEFAULT(capacity >= 0);

    xpString string = xp_string_alloc(allocator, capacity);
    if (cast(char *)(string) == NULL) {
        return string;
    }

    isize count = 0;
    if (str != NULL) {
        count = cast(isize)strlen(cast(char const *)str);
        if (count > capacity) {
            count = capacity;
        }
        memcpy(string, str, cast(size_t)count);
    }

    string[count] = '\0';
    XP_STRING_META(string)->count = count;

    return string;
}

xpString xp_make_string_count(xpAllocator allocator, char const *str, isize count) {
    XP_ASSERT_DEFAULT(count >= 0);

    xpString string = xp_string_alloc(allocator, count);
    if (cast(char *)(string) == NULL) {
        return string;
    }

    if (str != NULL && count > 0) {
        memcpy(string, str, cast(size_t)count);
    }

    string[count] = '\0';
    XP_STRING_META(string)->count = count;

    return string;
}

xpString xp_make_string_zero() {
#if defined(__cplusplus)
    return xpString();
#else
    xpString string;
    string.c_str = NULL;
    return string;
#endif
}

xpString xp_string_to_c_style(xpString string, xpAllocator allocator) {
    return xp_make_string_count(allocator, string, xp_string_count(string));
}


void xp_string_free(xpString string) {
    if (cast(char *)(string) == NULL) {
        return;
    }

    xpStringMetaData *meta = XP_STRING_META(string);
    xp_free(meta->allocator, meta);
}

b32 xp_string_cmp(xpString a, xpString b) {
    isize a_count = xp_string_count(a);
    isize b_count = xp_string_count(b);
    if (a_count > b_count) {
        return 1;
    } else if (a_count < b_count) {
        return -1;
    }

    if (a_count == 0) {
        return 0;
    }
    return memcmp(a, b, cast(size_t)a_count);
}

b32 xp_string_equal(xpString a, xpString b) {
    return xp_string_cmp(a, b) == 0;
}

xpString xp_string_copy(xpAllocator allocator, xpString string) {
    isize count = xp_string_count(string);

    xpString string_copy = xp_string_alloc(allocator, count);
    if (cast(char *)(string_copy) == NULL) {
        return string_copy;
    }

    if (count > 0) {
        memcpy(string_copy, string, cast(size_t)count);
    }
    string_copy[count] = '\0'; // 末尾补上'\0'
    XP_STRING_META(string_copy)->count = count;

    return string_copy;
}


xpString *xp_string_extend(xpString *string, isize extended_size) {
    if (extended_size <= 0 || string == NULL || cast(char *)(*string) == NULL) {
        return string;
    }

    xpStringMetaData *meta = XP_STRING_META(*string);
    isize count = meta->count;
    isize new_capacity = meta->capacity + extended_size;
    xpString new_string = xp_string_alloc(meta->allocator, new_capacity);
    if (cast(char *)(new_string) == NULL) {
        return string;
    }

    if (count > 0) {
        memcpy(new_string, *string, cast(size_t)count);
    }
    new_string[count] = '\0'; // 末尾补上'\0'
    XP_STRING_META(new_string)->count = count;

    xp_string_free(*string);
    *string = new_string;

    return string;
}

isize xp_string_find_char(xpString str, char c) {
    isize count = xp_string_count(str);
    for (isize i = 0; i < count; i++) {
        if (str[i] == c) {
            return i;
        }
    }
    return -1;
}


// NOTE(xoaop): cast(char *)(*str) 必须非空, data_count > 0
static void xp_string_insert_raw(isize pos, xpString *str, void const *data, isize data_count) {
    isize count = xp_string_count(*str);
    isize capacity = xp_string_capacity(*str);

    if (count + data_count > capacity) {
        xp_string_extend(str, count + data_count - capacity);
    }

    char *chars = cast(char *)(*str);

    if (pos < count) {
        memmove(chars + pos + data_count, chars + pos, cast(size_t)(count - pos));
    }
    memcpy(chars + pos, data, cast(size_t)data_count);

    isize new_count = count + data_count;
    chars[new_count] = '\0';
    XP_STRING_META(*str)->count = new_count;
}

xpString *xp_string_insert(isize pos, xpString *str, xpString inserted_str) {
    XP_ASSERT_DEFAULT(str != NULL);
    XP_ASSERT_DEFAULT(pos >= 0);
    XP_ASSERT_DEFAULT(pos <= xp_string_count(*str));

    isize inserted_count = xp_string_count(inserted_str);
    if (inserted_count <= 0) {
        return str;
    }

    if (cast(char *)(*str) == NULL) {
        *str = xp_string_copy(XP_STRING_META(inserted_str)->allocator, inserted_str);
        return str;
    }

    xp_string_insert_raw(pos, str, inserted_str, inserted_count);

    return str;
}

xpString *xp_string_append_char(xpString *str, char c) {
    if (str == NULL) {
        return str;
    }

    if (cast(char *)(*str) == NULL) {
        *str = xp_make_string_count(xp_default_allocator(), &c, 1);
        return str;
    }

    xp_string_insert_raw(xp_string_count(*str), str, &c, 1);

    return str;
}

xpString *xp_string_append(xpString *str, xpString appended_str) {
    if (str == NULL) {
        return str;
    }
    return xp_string_insert(xp_string_count(*str), str, appended_str);
}

xpString *xp_string_append_c(xpString *str, char const *appended_str) {
    if (str == NULL || appended_str == NULL) {
        return str;
    }

    isize appended_count = cast(isize)strlen(appended_str);
    if (appended_count <= 0) {
        return str;
    }

    if (cast(char *)(*str) == NULL) {
        *str = xp_make_string_count(xp_default_allocator(), appended_str, appended_count);
        return str;
    }

    xp_string_insert_raw(xp_string_count(*str), str, appended_str, appended_count);

    return str;
}


xpString xp_isize_to_string(isize value, xpAllocator allocator) {
    isize len = snprintf(NULL, 0, "%lld", value);

    xpString string = xp_make_string_capacity(allocator, NULL, len);
    if (cast(char *)(string) == NULL) {
        return string;
    }
    snprintf(string, cast(size_t)(len + 1), "%lld", value);

    XP_STRING_META(string)->count = len;

    return string;
}

xpString xp_string_replace_char(xpString string, char old_char, char new_char, xpAllocator allocator) {
    xpString new_string = xp_string_copy(allocator, string);

    isize count = xp_string_count(new_string);
    for (isize i = 0; i < count; i++) {
        if (new_string[i] == old_char) {
            new_string[i] = new_char;
        }
    }

    return new_string;
}


xpString xp_make_string_from_slice(xpAllocator allocator, xpSlice slice) {
    return xp_make_string_capacity(allocator, slice.data, slice.len);
}


xpSlice xp_slice_make(void *data, isize len) {
    xpSlice slice;
    slice.data = data;
    slice.len = len;
    return slice;
}

xpSlice xp_slice_make_from_string(xpString string, isize begin, isize len) {
    isize string_count = xp_string_count(string);
    if ((len < 0 || begin < 0) || ((begin + len) > string_count)) {
        XP_ASSERT(0);
    }

    xpSlice slice = {};
    slice.data = cast(char *)(string) + begin;
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

xpString::xpString() {
    c_str = NULL;
}

xpString::xpString(char const *str) {
    *this = xp_make_string(xp_default_allocator(), str);
}

xpString::xpString(xpAllocator allocator, char const *str) {
    *this = xp_make_string(allocator, str);
}

xpString::operator char *() const {
    return c_str;
}

bool xpString::operator==(xpString other) const {
    return xp_string_equal(*this, other) != 0;
}

bool xpString::operator!=(xpString other) const {
    return !(*this == other);
}

bool xpString::operator==(char const *other) const {
    return xp_string_equal(*this, other) != 0;
}

bool xpString::operator!=(char const *other) const {
    return !(*this == other);
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
