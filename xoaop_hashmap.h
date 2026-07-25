/*
    xoaop_hashmap.h — HashMap<K, V>

    依赖: xoaop.h, xoaop_hash.h
*/

#ifndef XOAOP_HASHMAP_H
#define XOAOP_HASHMAP_H

#if !defined(XOAOP_H)
#error "xoaop_hashmap.h requires xoaop.h to be included first"
#endif

#if defined(__cplusplus)

template<typename K, typename V>
struct xpHashMapEntry {
    K key;
    V value;
    xpHashSlotState state;
};

template<typename K, typename V>
struct xpHashMap {

    xpAllocator allocator;
    xpHashMapEntry<K, V> *entries;

    isize count;
    isize capacity;
};


template<typename K, typename V>
xpHashMap<K, V> xp_hash_map_make(xpAllocator allocator) {
    xpHashMap<K, V> hash_map = {};
    hash_map.allocator = allocator;
    hash_map.entries = NULL;

    hash_map.count = 0;
    hash_map.capacity = 0;

    return hash_map;
}

template<typename K, typename V>
void xp_hash_map_free(xpHashMap<K, V> map) {
    if (map.entries != NULL) {
        // 析构所有正在使用的元素
        for (isize i = 0; i < map.capacity; ++i) {
            if (map.entries[i].state == XP_HASH_SLOT_USED) {
                map.entries[i].key.~K();
                map.entries[i].value.~V();
            }
        }
        xp_free(map.allocator, map.entries);
    }
}

template<typename K, typename V>
xpHashMap<K, V> xp_hash_map_copy(xpHashMap<K, V> *o, xpAllocator allocator) {
    xpHashMap<K, V> copy = {};
    copy.allocator = allocator;
    copy.count = o->count;
    copy.capacity = o->capacity;

    if (o->capacity > 0 && o->entries != NULL) {
        copy.entries = xp_alloc_array<xpHashMapEntry<K, V>>(allocator, copy.capacity);
        // 只初始化state字段
        for (isize i = 0; i < copy.capacity; ++i) {
            copy.entries[i].state = XP_HASH_SLOT_EMPTY;
        }

        // 深拷贝每个元素
        for (isize i = 0; i < o->capacity; ++i) {
            const xpHashMapEntry<K, V> *src_entry = &o->entries[i];
            xpHashMapEntry<K, V> *dst_entry = &copy.entries[i];

            dst_entry->state = src_entry->state;
            if (src_entry->state == XP_HASH_SLOT_USED) {
                // 拷贝构造元素
                new (&dst_entry->key) K(src_entry->key);
                new (&dst_entry->value) V(src_entry->value);
            }
        }
    }

    return copy;
}



template<typename K, typename V>
void xp_hash_map_extend(xpHashMap<K, V> *map, isize new_capacity) {
    XP_ASSERT(new_capacity > map->capacity);

    if (map->entries == NULL) {
        map->entries = (xpHashMapEntry<K, V> *) xp_alloc(map->allocator, sizeof(xpHashMapEntry<K, V>) * new_capacity);
        // 只初始化state字段，不触碰key和value的内存（它们还未构造）
        for (isize i = 0; i < new_capacity; ++i) {
            map->entries[i].state = XP_HASH_SLOT_EMPTY;
        }
    } else {
        // Rehash
        xpHashMapEntry<K, V> *new_entries = (xpHashMapEntry<K, V> *) xp_alloc(map->allocator, sizeof(xpHashMapEntry<K, V>) * new_capacity);
        // 只初始化state字段
        for (isize i = 0; i < new_capacity; ++i) {
            new_entries[i].state = XP_HASH_SLOT_EMPTY;
        }

        for (isize i = 0; i < map->capacity; ++i) {
            xpHashMapEntry<K, V> *old_entry = &map->entries[i];
            if (old_entry->state == XP_HASH_SLOT_USED) { // 只rehash正在使用的条目，忽略墓碑
                usize hash_value = xp_hash_func(&old_entry->key);
                usize index = hash_value % new_capacity;
                // 线性探测
                while (new_entries[index].state == XP_HASH_SLOT_USED) {
                    index = (index + 1) % new_capacity;
                }
                // 使用移动构造转移资源所有权
                new (&new_entries[index].key) K(std::move(old_entry->key));
                new (&new_entries[index].value) V(std::move(old_entry->value));
                new_entries[index].state = XP_HASH_SLOT_USED;

                // 析构原位置的元素
                old_entry->key.~K();
                old_entry->value.~V();
                old_entry->state = XP_HASH_SLOT_EMPTY;
            }
        }
        xp_free(map->allocator, map->entries);

        map->entries = new_entries;
    }

    map->capacity = new_capacity;
    return;
}


// HashMap专用的线性探测封装
template<typename K, typename V>
LinearProbeResult xp_hash_map_linear_probe(xpHashMap<K, V> map, K key) {
    if (map.capacity == 0 || map.entries == nullptr) {
        return {};
    }

    usize hash_value = xp_hash_func(&key);


    return linear_probe(hash_value, map.capacity, [&](isize index, bool* out_key_match) {
        xpHashMapEntry<K, V>* entry = &map.entries[index];
        // 只有已使用的条目才能比较key
        *out_key_match = false;
        if (entry->state == XP_HASH_SLOT_USED) {
            *out_key_match = (entry->key == key);
        }
        return entry->state;
    });
}


template<typename K, typename V>
V *xp_hash_map_insert(xpHashMap<K, V> *map, K key, V value) {
    // 负载因子70%时扩容，避免哈希冲突过多
    if (map->capacity == 0 || (double)map->count / (double)map->capacity >= 0.7) {
        xp_hash_map_extend(map, map->capacity + map->capacity / 2 + 1);
    }

    auto probe_result = xp_hash_map_linear_probe(*map, key);

    // 如果key已经存在，更新值
    if (probe_result.found_index != -1) {
        xpHashMapEntry<K, V>* entry = &map->entries[probe_result.found_index];
        entry->value = value;
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
    XP_ASSERT(insert_index != -1 && "Hash map is full after expansion");
    if (insert_index == -1) {
        return nullptr;
    }

    xpHashMapEntry<K, V>* insert_entry = &map->entries[insert_index];
    // 使用placement new构造新元素
    new (&insert_entry->key) K(key);
    new (&insert_entry->value) V(value);
    insert_entry->state = XP_HASH_SLOT_USED;

    map->count += 1;
    return &insert_entry->value;
}



template<typename K, typename V>
xpHashMapEntry<K, V> *xp_hash_map_get_entry(xpHashMap<K, V> map, K key) {
    if (map.count == 0 || map.capacity == 0 || map.entries == nullptr) {
        return NULL;
    }

    auto probe_result = xp_hash_map_linear_probe(map, key);
    if (probe_result.found_index == -1) {
        return nullptr;
    }
    return &map.entries[probe_result.found_index];
}



template<typename K, typename V>
V *xp_hash_map_get(xpHashMap<K, V> map, K key) {
    xpHashMapEntry<K, V> *entry = xp_hash_map_get_entry(map, key);
    return entry ? &entry->value : nullptr;
}


template<typename K, typename V>
V *xp_hash_map_set(xpHashMap<K, V> *map, K key, V value) {
    xpHashMapEntry<K, V> *entry;
    if ((entry = xp_hash_map_get_entry(*map, key)) != NULL) {
        entry->value = value;
        return &entry->value;
    }
    return NULL;
}


template<typename K, typename V>
b32 xp_hash_map_remove(xpHashMap<K, V> *map, K key) {
    xpHashMapEntry<K, V> *entry;
    if ((entry = xp_hash_map_get_entry(*map, key)) != NULL) {
        // 显式析构元素资源
        entry->key.~K();
        entry->value.~V();
        entry->state = XP_HASH_SLOT_TOMBSTONE; // 标记为墓碑，而不是直接清空
        map->count -= 1;
        return true;
    }
    return false;
}


// 清空哈希表所有元素，保留容量
template<typename K, typename V>
void xp_hash_map_clear(xpHashMap<K, V> *map) {
    if (map->entries == NULL) {
        return;
    }

    for (isize i = 0; i < map->capacity; ++i) {
        xpHashMapEntry<K, V> *entry = &map->entries[i];
        if (entry->state == XP_HASH_SLOT_USED) {
            entry->key.~K();
            entry->value.~V();
            entry->state = XP_HASH_SLOT_EMPTY;
        } else if (entry->state == XP_HASH_SLOT_TOMBSTONE) {
            entry->state = XP_HASH_SLOT_EMPTY;
        }
    }

    map->count = 0;
}


template<typename K, typename V>
isize xp_hash_map_first_entry(xpHashMap<K, V> *map, xpHashMapEntry<K, V> **first_entry) {
    for (isize i = 0; i < map->capacity; i++) {
        if (map->entries[i].state == XP_HASH_SLOT_USED) {
            *first_entry = &map->entries[i];
            return i;
        }
    }

    *first_entry = NULL;
    return END_OF_HASH_MAP_INDEX;
}


template<typename K, typename V>
isize xp_hash_map_next_entry(xpHashMap<K, V> *map, isize curr_pos, xpHashMapEntry<K, V> **next_entry) {
    for (isize i = curr_pos + 1; i < map->capacity; i++) {
        if (map->entries[i].state == XP_HASH_SLOT_USED) {
            *next_entry = &map->entries[i];
            return i;
        }
    }

    *next_entry = NULL;
    return END_OF_HASH_MAP_INDEX;
}


template<typename K, typename V>
isize xp_hash_map_first_entry(const xpHashMap<K, V> *map, const xpHashMapEntry<K, V> **first_entry) {
    for (isize i = 0; i < map->capacity; i++) {
        if (map->entries[i].state == XP_HASH_SLOT_USED) {
            *first_entry = &map->entries[i];
            return i;
        }
    }

    *first_entry = NULL;
    return END_OF_HASH_MAP_INDEX;
}


template<typename K, typename V>
isize xp_hash_map_next_entry(const xpHashMap<K, V> *map, isize curr_pos, const xpHashMapEntry<K, V> **next_entry) {
    for (isize i = curr_pos + 1; i < map->capacity; i++) {
        if (map->entries[i].state == XP_HASH_SLOT_USED) {
            *next_entry = &map->entries[i];
            return i;
        }
    }

    *next_entry = NULL;
    return END_OF_HASH_MAP_INDEX;
}

#endif // __cplusplus

#endif // XOAOP_HASHMAP_H
