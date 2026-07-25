/*
    xoaop_hash.h — 哈希函数 & 公共哈希基础设施

    包含: murmur hash3, hash_combine, xp_hash_func 模板,
          xpHashSlotState, LinearProbeResult, linear_probe 通用探测算法,
          及所有内置类型的 hash 特化

    依赖: xoaop.h, xoaop_string.h (xpString hash 特化需要)
*/

#ifndef XOAOP_HASH_H
#define XOAOP_HASH_H

#if !defined(XOAOP_H)
#error "xoaop_hash.h requires xoaop.h to be included first"
#endif

#include <string>
#include <functional>

/*
    哈希映射函数 (C API)
*/

#if defined(__cplusplus)
extern "C" {
#endif

xp_define u32 xp_murmur_hash3_32(const void *data, usize length, usize seed);
xp_define u64 xp_hash_combine_u64(u64 old_hash, u64 new_value);

#if defined(__cplusplus)
}
#endif


#if defined(__cplusplus)

// hash函数接口定义
template<typename K>
usize xp_hash_func(K *key);

template<typename K>
usize xp_hash_func(const K *key);

template<typename T>
usize xp_hash_func(T **key) {
    return reinterpret_cast<usize>(*key);
}


// 哈希表槽位状态
typedef enum xpHashSlotState {
    XP_HASH_SLOT_EMPTY = 0,    // 空槽位
    XP_HASH_SLOT_USED = 1,     // 已使用
    XP_HASH_SLOT_TOMBSTONE = 2 // 墓碑：已删除，探测时跳过但不终止
} xpHashSlotState;


// 通用线性探测结果（不依赖具体条目类型）
struct LinearProbeResult {
    isize found_index = -1;       // 找到的匹配条目的索引（-1表示未找到）
    isize first_tombstone = -1;   // 第一个遇到的墓碑位置的索引（-1表示没有）
    isize first_empty = -1;       // 第一个空槽位置的索引（-1表示没有）
};


// 纯算法层面的通用线性探测：不依赖具体数据结构，只需要探测回调
// 回调函数签名：xpHashSlotState probe_callback(isize index, bool* out_key_match)
// 返回值：当前索引的槽位状态；out_key_match输出当前索引的key是否匹配目标key
template<typename ProbeCallback>
LinearProbeResult linear_probe(usize hash_value, isize capacity, ProbeCallback&& callback) {
    LinearProbeResult result = {};

    if (capacity <= 0) {
        return result;
    }

    usize index = hash_value % static_cast<usize>(capacity);
    usize original_index = index;

    // 至少执行一次探测
    for (;;) {
        bool key_match = false;
        xpHashSlotState state = callback(static_cast<isize>(index), &key_match);

        if (state == XP_HASH_SLOT_EMPTY) {
            // 记录第一个空槽位置
            if (result.first_empty == -1) {
                result.first_empty = static_cast<isize>(index);
            }

            // 探测链结束，没有找到匹配
            break;
        } else if (state == XP_HASH_SLOT_TOMBSTONE) {
            // 记录第一个墓碑位置
            if (result.first_tombstone == -1) {
                result.first_tombstone = static_cast<isize>(index);
            }

        } else if (state == XP_HASH_SLOT_USED && key_match) {
            // 找到匹配的key
            result.found_index = static_cast<isize>(index);
            break;
        }

        index = (index + 1) % static_cast<usize>(capacity);

        // 防止无限循环
        if (index == original_index) {
            break;
        }
    }

    return result;
}

#define END_OF_HASH_MAP_INDEX -1

#endif // __cplusplus


#if defined(XOAOP_IMPLEMENTATION) && !defined(XOAOP_HASH_IMPLEMENTATION_DONE)
#define XOAOP_HASH_IMPLEMENTATION_DONE

#if defined(__cplusplus)
extern "C" {
#endif

//
// 哈希映射函数实现
//


//NOTE: 可以移到常用函数里
xp_internal u32 rotate_left(u32 value, i32 shift) {
    const i32 bits = sizeof(u32) * 8;  // 32 位
    shift %= bits;  // 确保 shift 在 0 到 31 之间
    return (value << shift) | (value >> (bits - shift));
}

u32 xp_murmur_hash3_32(const void *data, usize length, usize seed) {
    const u8 *key = cast(const u8 *)(data);
    const usize nblocks = length / 4;

    u32 h1 = seed;

    const u32 c1 = 0xcc9e2d51;
    const u32 c2 = 0x1b873593;

    // 处理 4 字节块
    const u32 *blocks = cast(u32 *)key;
    for (usize i = 0; i < nblocks; ++i) {
        u32 k1 = blocks[i];

        k1 *= c1;
        k1 = rotate_left(k1, 15);
        k1 *= c2;

        h1 ^= k1;
        h1 = rotate_left(h1, 13);
        h1 = h1 * 5 + 0xe6546b64;
    }

    // 处理剩余字节
    const u8 *tail = key + nblocks * 4;
    u32 k1 = 0;

    switch (length & 3) {
    case 3: k1 ^= tail[2] << 16;
    case 2: k1 ^= tail[1] << 8;
    case 1: k1 ^= tail[0];
        k1 *= c1;
        k1 = rotate_left(k1, 15);
        k1 *= c2;
        h1 ^= k1;
    }

    // 最终混合
    h1 ^= length;
    h1 ^= h1 >> 16;
    h1 *= 0x85ebca6b;
    h1 ^= h1 >> 13;
    h1 *= 0xc2b2ae35;
    h1 ^= h1 >> 16;

    return h1;
}

// 一个简单的hash combine函数
u64 xp_hash_combine_u64(u64 old_hash, u64 new_value) {
    static const u64 HASH_SEED = 0x517cc1b727220a95;
    static const u64 K = 0x9e3779b97f4a7c15;

    u64 z = new_value + K + (old_hash << 6) + (old_hash >> 2);
    z ^= (z >> 33);
    z *= 0xff51afd7ed558ccd;
    z ^= (z >> 33);
    z *= 0xc4ceb9fe1a85ec53;
    z ^= (z >> 33);
    return old_hash ^ z;
}

#if defined(__cplusplus)
}
#endif


#if defined(__cplusplus)

//
// 常见类型的hash函数实现
//

template<>
usize xp_hash_func<i32>(i32 *key) {
    return xp_murmur_hash3_32(key, sizeof(i32), 0);
}

template<>
usize xp_hash_func<i64>(i64 *key) {
    return xp_murmur_hash3_32(key, sizeof(i64), 0);
}

template<>
usize xp_hash_func<u32>(u32 *key) {
    return xp_murmur_hash3_32(key, sizeof(u32), 0);
}

template<>
usize xp_hash_func<u64>(u64 *key) {
    return xp_murmur_hash3_32(key, sizeof(u64), 0);
}

template<>
usize xp_hash_func<f32>(f32 *key) {
    u32 bits;
    memcpy(&bits, key, sizeof(f32));
    return xp_murmur_hash3_32(&bits, sizeof(u32), 0);
}

template<>
usize xp_hash_func<f64>(f64 *key) {
    u64 bits;
    memcpy(&bits, key, sizeof(f64));
    return xp_murmur_hash3_32(&bits, sizeof(u64), 0);
}

template<>
usize xp_hash_func<char>(char *key) {
    return cast(usize)(*key);
}

template<>
usize xp_hash_func<xpString>(xpString *key) {
    return xp_murmur_hash3_32(key->c_str, cast(usize) key->length, 0);
}

template<>
usize xp_hash_func<const std::string>(const std::string *key) {
    std::hash<std::string> hasher;
    return hasher(*key);
}

template<>
usize xp_hash_func<std::string>(std::string *key) {
    std::hash<std::string> hasher;
    return hasher(*key);
}

template<>
usize xp_hash_func<void*>(void** key) {
    return cast(usize)(*key);
}

#endif // __cplusplus

#endif // XOAOP_IMPLEMENTATION

#endif // XOAOP_HASH_H
