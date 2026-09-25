/*
    xoaop_intern.h — InterningTable<T, CAPACITY>

    用于任意类型的唯一化存储, 保证每个值只存储一份, 且指针永不失效

    依赖: xoaop.h, xoaop_hash.h
*/

#ifndef XOAOP_INTERN_H
#define XOAOP_INTERN_H

#if !defined(XOAOP_H)
#error "xoaop_intern.h requires xoaop.h to be included first"
#endif

#if defined(__cplusplus)

template<typename T>
struct xpInterningEntry {
    T *key_ptr;
    b8 used;
};


template<typename T, size_t CAPACITY>
struct xpInterningTable {

    xpArena arena; // 只能是独立的ArenaAllocator
    xpAllocator allocator; // 只能是独立的ArenaAllocator

    xpInterningEntry<T> buckets[CAPACITY];

    isize count;


    static constexpr size_t capacity = CAPACITY;
    static_assert((CAPACITY & (CAPACITY - 1)) == 0, "InterningTable CAPACITY must be a power of 2");
};

template<typename T, size_t CAPACITY>
void xp_interning_table_init(xpInterningTable<T, CAPACITY> *table) {
    xp_arena_init_default(&table->arena);
    table->allocator = xp_arena_allocator(&table->arena);

    for (isize i = 0; i < CAPACITY; ++i) {
        table->buckets[i].used = false;
    }
    table->count = 0;
}

template<typename T, size_t CAPACITY>
void xp_interning_table_free(xpInterningTable<T, CAPACITY> *table) {
    xp_free_all(table->allocator);
}


template<typename T, size_t CAPACITY>
LinearProbeResult xp_interning_table_linear_probe(xpInterningTable<T, CAPACITY> *table, T key) {
    usize hash_value = std::hash<T>{}(key);
    return linear_probe(hash_value, xpInterningTable<T, CAPACITY>::capacity, [&](isize index, bool* out_key_match) {
        xpInterningEntry<T>* entry = &table->buckets[index];
        // 只有已使用的条目才能比较key
        *out_key_match = false;
        if (entry->used) {
            *out_key_match = (*(entry->key_ptr) == key);
        }
        return entry->used ? XP_HASH_SLOT_USED : XP_HASH_SLOT_EMPTY;
    });
}


template<typename T, size_t CAPACITY>
xpInterningEntry<T> *xp_interning_table_get_entry(xpInterningTable<T, CAPACITY> *table, T key) {
    if(table->count == 0) {
        return NULL;
    }

    auto probe_result = xp_interning_table_linear_probe(table, key);
    if(probe_result.found_index == -1) {
        return nullptr;
    }

    return &table->buckets[probe_result.found_index];
}


template<typename T, size_t CAPACITY>
T *xp_interning_table_get(xpInterningTable<T, CAPACITY> *table, T key) {
    xpInterningEntry<T> *entry = xp_interning_table_get_entry(table, key);
    return entry ? entry->key_ptr : NULL;
}


template<typename T, size_t CAPACITY>
T *xp_interning_table_insert(xpInterningTable<T, CAPACITY> *table, T key) {
    if (table->count >= table->capacity) {
        return NULL; // 满了
    }

    auto probe_result = xp_interning_table_linear_probe(table, key);

    if(probe_result.found_index == -1){
        xpInterningEntry<T> *entry = &table->buckets[probe_result.first_empty];

        T *stored_key_ptr = xp_alloc<T>(table->allocator);
        *stored_key_ptr = key;

        entry->key_ptr = stored_key_ptr;
        entry->used = true;

        table->count += 1;
        return stored_key_ptr;
    } else {
        return nullptr; // 重复插入当作失败处理
    }

    //NOTE: FULL MAP
    XP_ASSERT_DEFAULT(0);
    return NULL;
}

#endif // __cplusplus

#endif // XOAOP_INTERN_H
