#define XOAOP_IMPLEMENTATION
#include "xoaop.h"

#include <cstdio>
#include <cstring>
#include <utility>
#include <cstdint>

static void test_string() {
    // 默认构造（值初始化归零）
    xpString z{};
    XP_ASSERT(z.length == 0);
    XP_ASSERT(z.c_str == nullptr);

    // const char* 构造 (无分配)
    xpString s = "hello world";
    XP_ASSERT(s.length == 11);
    XP_ASSERT(xp_string_cmp(s, "hello world") == 0);
    XP_ASSERT(xp_string_equal(s, "hello world"));

    // xpAllocator + const char* 构造 (分配拷贝)
    xpString s2(xp_heap_allocator(), "test");
    XP_ASSERT(s2.length == 4);
    XP_ASSERT(s2.c_str[0] == 't');
    xp_string_free(s2);

    // xp_string_append
    xpString s3(xp_heap_allocator(), "hello");
    xp_string_append(&s3, " world");
    XP_ASSERT(xp_string_equal(s3, "hello world"));
    xp_string_free(s3);

    // xp_string_find_char
    isize idx = xp_string_find_char("abcde", 'c');
    XP_ASSERT(idx == 2);

    // xp_string_replace_char
    xpString s5 = xp_string_replace_char("a,b,c", ',', ';', xp_heap_allocator());
    XP_ASSERT(xp_string_equal(s5, "a;b;c"));
    xp_string_free(s5);

    // xp_slice
    xpSlice slice = xp_slice_make_from_string("hello", 1, 3);
    XP_ASSERT(slice.len == 3);
    XP_ASSERT(((char*)slice.data)[0] == 'e');

    // xp_make_string_from_slice
    xpString from_slice = xp_make_string_from_slice(xp_heap_allocator(), slice);
    XP_ASSERT(xp_string_equal(from_slice, "ell"));
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
    auto pair = xp_make_pair(1, "one");
    XP_ASSERT(pair.first == 1);
    XP_ASSERT(xp_string_equal(pair.second, "one"));

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
    xp_hash_map_insert(&map, 1, xpString("one"));
    xp_hash_map_insert(&map, 2, xpString("two"));
    xp_hash_map_insert(&map, 3, xpString("three"));

    // get
    xpString* val = xp_hash_map_get(map, 2);
    XP_ASSERT(val != nullptr);
    XP_ASSERT(xp_string_equal(*val, "two"));

    // not found
    val = xp_hash_map_get(map, 99);
    XP_ASSERT(val == nullptr);

    // update
    xp_hash_map_insert(&map, 1, xpString("ONE"));
    val = xp_hash_map_get(map, 1);
    XP_ASSERT(xp_string_equal(*val, "ONE"));

    // remove
    bool removed = xp_hash_map_remove(&map, 2);
    XP_ASSERT(removed);
    val = xp_hash_map_get(map, 2);
    XP_ASSERT(val == nullptr);

    // iteration (range-for)
    isize count = 0;
    for (const auto& entry : map) {
        count++;
    }
    XP_ASSERT(count == 2);

    // operator[] read
    xpString& v = map[3];
    XP_ASSERT(xp_string_equal(v, "three"));

    // operator[] write
    map[3] = xpString("THREE");
    XP_ASSERT(xp_string_equal(map[3], "THREE"));

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

    // iterator (range-for)
    isize count = 0;
    for (const auto& entry : set) {
        count++;
    }
    XP_ASSERT(count == 2);

    // contains
    XP_ASSERT(set.contains(10));
    XP_ASSERT(!set.contains(99));

    // operator[]
    XP_ASSERT(set[10] == 10);

    xp_hash_set_free(set);

    std::printf("[OK] test_hashset\n");
}

static void test_hash() {
    // std::hash consistency: same key → same hash
    i32 key1 = 42;
    usize h1 = std::hash<i32>{}(key1);
    usize h2 = std::hash<i32>{}(key1);
    XP_ASSERT(h1 == h2);

    // murmur hash consistency
    u32 m1 = xp_murmur_hash3_32(&key1, sizeof(i32), 0);
    u32 m2 = xp_murmur_hash3_32(&key1, sizeof(i32), 0);
    XP_ASSERT(m1 == m2);

    // hash combine
    u64 combined = xp_hash_combine_u64(0, 12345);
    XP_ASSERT(combined != 0);

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
    XP_ASSERT(p2 != p1);

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
    xpString a(xp_heap_allocator(), "start");
    xpString b(xp_heap_allocator(), "end");

    // with middle
    xpString result = xp_string_concat_mid(a, b, xpOption<xpString>::some("-"), xp_heap_allocator());
    XP_ASSERT(xp_string_equal(result, "start-end"));
    xp_string_free(result);

    // without middle
    xpString result2 = xp_string_concat_mid(a, b, xpOption<xpString>::none(), xp_heap_allocator());
    XP_ASSERT(xp_string_equal(result2, "startend"));
    xp_string_free(result2);

    xp_string_free(a);
    xp_string_free(b);

    std::printf("[OK] test_string_concat_mid\n");
}

// murmur3_x86_32 的可移植参考实现（逐位对齐，专门用 memcpy 读，天然支持未对齐输入）
static u32 murmur3_reference(const void *data, usize length, u32 seed) {
    const u8 *key = static_cast<const u8 *>(data);
    const usize nblocks = length / 4;

    u32 h1 = seed;
    const u32 c1 = 0xcc9e2d51;
    const u32 c2 = 0x1b873593;

    for (usize i = 0; i < nblocks; ++i) {
        u32 k1;
        std::memcpy(&k1, key + i * 4, 4);

        k1 *= c1;
        k1 = (k1 << 15) | (k1 >> 17);
        k1 *= c2;

        h1 ^= k1;
        h1 = (h1 << 13) | (h1 >> 19);
        h1 = h1 * 5 + 0xe6546b64;
    }

    const u8 *tail = key + nblocks * 4;
    u32 k1 = 0;
    switch (length & 3) {
        case 3: k1 ^= static_cast<u32>(tail[2]) << 16; [[fallthrough]];
        case 2: k1 ^= static_cast<u32>(tail[1]) << 8;  [[fallthrough]];
        case 1:
            k1 ^= static_cast<u32>(tail[0]);
            k1 *= c1;
            k1 = (k1 << 15) | (k1 >> 17);
            k1 *= c2;
            h1 ^= k1;
    }

    h1 ^= static_cast<u32>(length);
    h1 ^= h1 >> 16;
    h1 *= 0x85ebca6b;
    h1 ^= h1 >> 13;
    h1 *= 0xc2b2ae35;
    h1 ^= h1 >> 16;
    return h1;
}

static void test_hash_murmur_unaligned() {
    u8 buf[64];
    for (usize i = 0; i < sizeof(buf); ++i) {
        buf[i] = static_cast<u8>(i * 7 + 3);
    }

    // 4 种起始偏移 × 0..40 字节长度，覆盖所有 tail 分支与 4 字节块边界。
    // 偏移 1..3 制造未对齐输入：旧实现强转 u32* 在此是 UB（UBSan 会报）。
    for (usize off = 0; off < 4; ++off) {
        for (usize len = 0; len <= 40; ++len) {
            const u32 got = xp_murmur_hash3_32(buf + off, len, 0x1234u);
            const u32 exp = murmur3_reference(buf + off, len, 0x1234u);
            XP_ASSERT(got == exp);
        }
    }

    std::printf("[OK] test_hash_murmur_unaligned\n");
}

static void test_arena_alignment() {
    // arena 必须按 xp_max_alignment() 对齐分配，否则含 __int128 的类型
    // （对齐 16）在 Release -O2 下会因 16 字节 movaps 访问未对齐地址而崩
    const isize align = xp_max_alignment();
    XP_ASSERT(align > 0);
    XP_ASSERT(xp_is_power_of_two(align));

    xpArena arena;
    xp_arena_init(&arena, 4096);

    xpAllocator allocator = xp_arena_allocator_default();
    allocator.data = &arena;

    // 递增的 size（非对齐倍数）会让 used 逐渐漂移，
    // 只要有一处漏了对齐，后面的分配就会暴露出来
    for (usize i = 0; i < 256; ++i) {
        const isize size = 1 + static_cast<isize>(i % 37);
        void *p = xp_alloc(allocator, size);
        XP_ASSERT(p != nullptr);
        XP_ASSERT(reinterpret_cast<uintptr_t>(p) % static_cast<uintptr_t>(align) == 0);
    }

    xp_arena_free_all(&arena);

    std::printf("[OK] test_arena_alignment (align=%lld)\n", static_cast<long long>(align));
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
    test_hash_murmur_unaligned();
    test_arena_alignment();
    test_intern();
    test_string_concat_mid();

    std::printf("\nAll tests passed!\n");
    return 0;
}
