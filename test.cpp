#define XOAOP_IMPLEMENTATION
#include "xoaop.h"

#include <cstdio>

static void test_string() {
    // xp_string_c
    xpString s = xp_string_c("hello world");
    XP_ASSERT(s.length == 11);
    XP_ASSERT(xp_string_cmp(s, xp_string_c("hello world")) == 0);
    XP_ASSERT(xp_string_equal(s, xp_string_c("hello world")));

    // xp_make_string
    xpString s2 = xp_make_string(xp_heap_allocator(), "test");
    XP_ASSERT(s2.length == 4);
    XP_ASSERT(s2.c_str[0] == 't');
    xp_string_free(s2);

    // xp_string_append
    xpString s3 = xp_make_string(xp_heap_allocator(), "hello");
    xp_string_append(&s3, xp_string_c(" world"));
    XP_ASSERT(xp_string_equal(s3, xp_string_c("hello world")));
    xp_string_free(s3);

    // xp_string_find_char
    isize idx = xp_string_find_char(xp_string_c("abcde"), 'c');
    XP_ASSERT(idx == 2);

    // xp_string_replace_char
    xpString s5 = xp_string_replace_char(xp_string_c("a,b,c"), ',', ';', xp_heap_allocator());
    XP_ASSERT(xp_string_equal(s5, xp_string_c("a;b;c")));
    xp_string_free(s5);

    // xp_slice
    xpSlice slice = xp_slice_make_from_string(xp_string_c("hello"), 1, 3);
    XP_ASSERT(slice.len == 3);
    XP_ASSERT(((char*)slice.data)[0] == 'e');

    // xp_make_string_from_slice
    xpString from_slice = xp_make_string_from_slice(xp_heap_allocator(), slice);
    XP_ASSERT(xp_string_equal(from_slice, xp_string_c("ell")));
    xp_string_free(from_slice);

    std::printf("[OK] test_string\n");
}

static void test_option() {
    auto some = xpOption<int>::some(42);
    XP_ASSERT(some.has_value());
    XP_ASSERT(!some.is_none());
    XP_ASSERT(some.unwrap() == 42);

    auto none = xpOption<int>::none();
    XP_ASSERT(none.is_none());
    XP_ASSERT(!none.has_value());

    std::printf("[OK] test_option\n");
}

static void test_result() {
    auto ok = xpResult<int, const char*>::ok(100);
    XP_ASSERT(ok.is_ok());
    XP_ASSERT(!ok.is_err());
    XP_ASSERT(ok.as_ok() == 100);

    auto err = xpResult<int, const char*>::err("fail");
    XP_ASSERT(err.is_err());

    // match
    bool matched_ok = false;
    bool matched_err = false;
    match(ok,
        [&](int v) { matched_ok = (v == 100); },
        [&](const char*) { matched_err = true; }
    );
    XP_ASSERT(matched_ok);
    XP_ASSERT(!matched_err);

    match(err,
        [&](int) {},
        [&](const char*) { matched_err = true; }
    );
    XP_ASSERT(matched_err);

    std::printf("[OK] test_result\n");
}

static void test_pair() {
    auto pair = xp_make_pair(1, xp_string_c("one"));
    XP_ASSERT(pair.first == 1);
    XP_ASSERT(xp_string_equal(pair.second, xp_string_c("one")));

    std::printf("[OK] test_pair\n");
}

static void test_defer() {
    int counter = 0;
    {
        defer(counter = 42);
        XP_ASSERT(counter == 0);
    }
    XP_ASSERT(counter == 42);

    std::printf("[OK] test_defer\n");
}

static void test_hashmap() {
    auto map = xp_hash_map_make<int, xpString>(xp_heap_allocator());

    // insert
    xp_hash_map_insert(&map, 1, xp_string_c("one"));
    xp_hash_map_insert(&map, 2, xp_string_c("two"));
    xp_hash_map_insert(&map, 3, xp_string_c("three"));

    // get
    xpString* val = xp_hash_map_get(map, 2);
    XP_ASSERT(val != nullptr);
    XP_ASSERT(xp_string_equal(*val, xp_string_c("two")));

    // not found
    val = xp_hash_map_get(map, 99);
    XP_ASSERT(val == nullptr);

    // update
    xp_hash_map_insert(&map, 1, xp_string_c("ONE"));
    val = xp_hash_map_get(map, 1);
    XP_ASSERT(xp_string_equal(*val, xp_string_c("ONE")));

    // remove
    bool removed = xp_hash_map_remove(&map, 2);
    XP_ASSERT(removed);
    val = xp_hash_map_get(map, 2);
    XP_ASSERT(val == nullptr);

    // iteration
    xpHashMapEntry<int, xpString>* entry = nullptr;
    isize pos = xp_hash_map_first_entry(&map, &entry);
    isize count = 0;
    while (pos != END_OF_HASH_MAP_INDEX) {
        count++;
        pos = xp_hash_map_next_entry(&map, pos, &entry);
    }
    XP_ASSERT(count == 2);

    xp_hash_map_free(map);

    std::printf("[OK] test_hashmap\n");
}

static void test_hashset() {
    auto set = xp_hash_set_make<int>(xp_heap_allocator());

    xp_hash_set_insert(&set, 10);
    xp_hash_set_insert(&set, 20);
    xp_hash_set_insert(&set, 30);

    XP_ASSERT(xp_hash_set_find(&set, 20));
    XP_ASSERT(!xp_hash_set_find(&set, 99));

    // duplicate insert
    int* dup = xp_hash_set_insert(&set, 10);
    XP_ASSERT(dup == nullptr);

    // get
    int* val = xp_hash_set_get(&set, 30);
    XP_ASSERT(val != nullptr && *val == 30);

    // remove
    bool removed = xp_hash_set_remove(&set, 20);
    XP_ASSERT(removed);
    XP_ASSERT(!xp_hash_set_find(&set, 20));

    xp_hash_set_free(set);

    std::printf("[OK] test_hashset\n");
}

static void test_hash() {
    i32 key1 = 42;
    usize h1 = xp_hash_func(&key1);
    usize h2 = xp_murmur_hash3_32(&key1, sizeof(i32), 0);
    // Same key should produce same hash
    XP_ASSERT(h1 == h2);

    // hash combine
    u64 combined = xp_hash_combine_u64(0, 12345);
    XP_ASSERT(combined != 0); // should produce non-zero result

    std::printf("[OK] test_hash\n");
}

static void test_intern() {
    constexpr isize N = 64;
    xpInterningTable<int, N> table;
    xp_interning_table_init(&table);

    int* p1 = xp_interning_table_insert(&table, 100);
    XP_ASSERT(p1 != nullptr);
    XP_ASSERT(*p1 == 100);

    int* p2 = xp_interning_table_insert(&table, 200);
    XP_ASSERT(p2 != nullptr);
    XP_ASSERT(p2 != p1); // different addresses

    // duplicate insert
    int* p3 = xp_interning_table_insert(&table, 100);
    XP_ASSERT(p3 == nullptr);

    // get
    int* g = xp_interning_table_get(&table, 200);
    XP_ASSERT(g != nullptr && *g == 200);

    xp_interning_table_free(&table);

    std::printf("[OK] test_intern\n");
}

static void test_string_concat_mid() {
    xpString a = xp_make_string(xp_heap_allocator(), "start");
    xpString b = xp_make_string(xp_heap_allocator(), "end");

    // with middle
    xpString result = xp_string_concat_mid(a, b, xpOption<xpString>::some(xp_string_c("-")), xp_heap_allocator());
    XP_ASSERT(xp_string_equal(result, xp_string_c("start-end")));
    xp_string_free(result);

    // without middle
    xpString result2 = xp_string_concat_mid(a, b, xpOption<xpString>::none(), xp_heap_allocator());
    XP_ASSERT(xp_string_equal(result2, xp_string_c("startend")));
    xp_string_free(result2);

    xp_string_free(a);
    xp_string_free(b);

    std::printf("[OK] test_string_concat_mid\n");
}

int main() {
    test_string();
    test_option();
    test_result();
    test_pair();
    test_defer();
    test_hashmap();
    test_hashset();
    test_hash();
    test_intern();
    test_string_concat_mid();

    std::printf("\nAll tests passed!\n");
    return 0;
}
