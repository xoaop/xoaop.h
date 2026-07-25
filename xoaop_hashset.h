/*
    xoaop_hashset.h — HashSet<K>

    依赖: xoaop.h, xoaop_hash.h
*/

#ifndef XOAOP_HASHSET_H
#define XOAOP_HASHSET_H

#if !defined(XOAOP_H)
#error "xoaop_hashset.h requires xoaop.h to be included first"
#endif

#if defined(__cplusplus)

template<typename K>
struct xpHashSetEntry {
    K key;
    xpHashSlotState state;
};


template<typename K>
struct xpHashSet {
    xpAllocator allocator;
    xpHashSetEntry<K> *entries;

    isize count;
    isize capacity;
};

template<typename K>
LinearProbeResult xp_hash_set_linear_probe(xpHashSet<K> set, K key) {
    if (set.capacity == 0 || set.entries == nullptr) {
        return {};
    }

    usize hash_value = xp_hash_func(&key);
    return linear_probe(hash_value, set.capacity, [&](isize index, bool* out_key_match) {
        xpHashSetEntry<K>* entry = &set.entries[index];
        // 只有已使用的条目才能比较key
        *out_key_match = false;
        if (entry->state == XP_HASH_SLOT_USED) {
            *out_key_match = (entry->key == key);
        }
        return entry->state;
    });
}


template<typename K>
xpHashSet<K> xp_hash_set_make(xpAllocator allocator) {
    xpHashSet<K> hash_set = {};
    hash_set.allocator = allocator;
    hash_set.entries = NULL;

    hash_set.count = 0;
    hash_set.capacity = 0;

    return hash_set;
}

template<typename K>
void xp_hash_set_free(xpHashSet<K> set) {
    if (set.entries != NULL) {
        // 析构所有正在使用的元素
        for (isize i = 0; i < set.capacity; ++i) {
            if (set.entries[i].state == XP_HASH_SLOT_USED) {
                set.entries[i].key.~K();
            }
        }
        xp_free(set.allocator, set.entries);
    }
}

template<typename K>
void xp_hash_set_clear(xpHashSet<K> *set) {
    if (set->entries == NULL || set->capacity <= 0) {
        set->count = 0;
        return;
    }

    // 析构所有正在使用的元素
    for (isize i = 0; i < set->capacity; ++i) {
        xpHashSetEntry<K> *entry = &set->entries[i];
        if (entry->state == XP_HASH_SLOT_USED) {
            entry->key.~K();
            entry->state = XP_HASH_SLOT_EMPTY;
        } else if (entry->state == XP_HASH_SLOT_TOMBSTONE) {
            entry->state = XP_HASH_SLOT_EMPTY;
        }
    }

    set->count = 0;
}

template<typename K>
xpHashSet<K> xp_hash_set_copy(xpHashSet<K> *set, xpAllocator allocator) {
    xpHashSet<K> copy = {};
    copy.allocator = allocator;
    copy.count = set->count;
    copy.capacity = set->capacity;

    if (set->capacity > 0 && set->entries != NULL) {
        copy.entries = xp_alloc_array<xpHashSetEntry<K>>(allocator, copy.capacity);
        // 只初始化state字段
        for (isize i = 0; i < copy.capacity; ++i) {
            copy.entries[i].state = XP_HASH_SLOT_EMPTY;
        }

        // 深拷贝每个元素
        for (isize i = 0; i < set->capacity; ++i) {
            const xpHashSetEntry<K> *src_entry = &set->entries[i];
            xpHashSetEntry<K> *dst_entry = &copy.entries[i];

            dst_entry->state = src_entry->state;
            if (src_entry->state == XP_HASH_SLOT_USED) {
                // 拷贝构造元素
                new (&dst_entry->key) K(src_entry->key);
            }
        }
    }

    return copy;
}


template<typename K>
void xp_hash_set_extend(xpHashSet<K> *set, isize new_capacity) {
    XP_ASSERT_DEFAULT(new_capacity > set->capacity);

    if (set->entries == NULL) {
        set->entries = (xpHashSetEntry<K> *) xp_alloc(set->allocator, sizeof(xpHashSetEntry<K>) * new_capacity);
        // 只初始化state字段，不触碰key的内存（它们还未构造）
        for (isize i = 0; i < new_capacity; ++i) {
            set->entries[i].state = XP_HASH_SLOT_EMPTY;
        }
    } else {
        // Rehash
        xpHashSetEntry<K> *new_entries = (xpHashSetEntry<K> *) xp_alloc(set->allocator, sizeof(xpHashSetEntry<K>) * new_capacity);
        // 只初始化state字段
        for (isize i = 0; i < new_capacity; ++i) {
            new_entries[i].state = XP_HASH_SLOT_EMPTY;
        }

        for (isize i = 0; i < set->capacity; ++i) {
            xpHashSetEntry<K> *old_entry = &set->entries[i];
            if (old_entry->state == XP_HASH_SLOT_USED) { // 只rehash正在使用的条目，忽略墓碑
                usize hash_value = xp_hash_func(&old_entry->key);
                usize index = hash_value % new_capacity;
                // 线性探测
                while (new_entries[index].state == XP_HASH_SLOT_USED) {
                    index = (index + 1) % new_capacity;
                }
                // 使用移动构造转移资源所有权
                new (&new_entries[index].key) K(std::move(old_entry->key));
                new_entries[index].state = XP_HASH_SLOT_USED;

                // 析构原位置的元素
                old_entry->key.~K();
                old_entry->state = XP_HASH_SLOT_EMPTY;
            }
        }
        xp_free(set->allocator, set->entries);

        set->entries = new_entries;
    }

    set->capacity = new_capacity;
    return;
}


template<typename K>
K *xp_hash_set_insert(xpHashSet<K> *set, K key) {
    // 负载因子70%时扩容，避免哈希冲突过多
    if (set->capacity == 0 || (double)set->count / (double)set->capacity >= 0.7) {
        xp_hash_set_extend(set, set->capacity + set->capacity / 2 + 1);
    }

    auto probe_result = xp_hash_set_linear_probe(*set, key);

    // 如果key已经存在，返回nullptr表示插入失败
    if (probe_result.found_index != -1) {
        return nullptr;
    }

    // 优先使用墓碑位置，其次使用空槽位置
    isize insert_index = -1;
    if (probe_result.first_tombstone != -1) {
        insert_index = probe_result.first_tombstone;
    } else if (probe_result.first_empty != -1) {
        insert_index = probe_result.first_empty;
    }

    // 理论上扩容后不可能没有空槽，这里做防御性检查
    XP_ASSERT_DEFAULT(insert_index != -1 && "Hash set is full after expansion");
    if (insert_index == -1) {
        return nullptr;
    }

    xpHashSetEntry<K>* insert_entry = &set->entries[insert_index];
    // 使用placement new构造新元素
    new (&insert_entry->key) K(key);
    insert_entry->state = XP_HASH_SLOT_USED;

    set->count += 1;
    return &insert_entry->key;
}

template<typename K>
xpHashSetEntry<K> *xp_hash_set_get_entry(xpHashSet<K> *set, K key) {
    if (set->count == 0 || set->capacity == 0 || set->entries == nullptr) {
        return NULL;
    }

    auto probe_result = xp_hash_set_linear_probe(*set, key);
    if (probe_result.found_index == -1) {
        return nullptr;
    }
    return &set->entries[probe_result.found_index];
}

template<typename K>
K *xp_hash_set_get(xpHashSet<K> *set, K key) {
    xpHashSetEntry<K> *entry = xp_hash_set_get_entry(set, key);
    return entry ? &entry->key : NULL;
}


template<typename K>
b32 xp_hash_set_find(xpHashSet<K> *set, K key) {
    xpHashSetEntry<K> *entry = NULL;
    if ((entry = xp_hash_set_get_entry(set, key)) != NULL) {
        return true;
    }

    return false;
}


template<typename K>
b32 xp_hash_set_remove(xpHashSet<K> *set, K key) {
    xpHashSetEntry<K> *entry = NULL;
    if ((entry = xp_hash_set_get_entry(set, key)) != NULL) {
        // 显式析构元素资源
        entry->key.~K();
        entry->state = XP_HASH_SLOT_TOMBSTONE; // 标记为墓碑，而不是直接清空
        set->count -= 1;
        return true;
    }

    return false;
}

#endif // __cplusplus

#endif // XOAOP_HASHSET_H
