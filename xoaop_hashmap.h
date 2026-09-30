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
    xpHashSlotState state;
    K key;
    V value;
};

template<typename K, typename V>
struct xpHashMapIterator;

template<typename K, typename V>
struct xpHashMap {

    xpAllocator allocator;
    xpHashMapEntry<K, V> *entries;
    isize count;
    isize occupied;   // 已占槽位 = 存活 + 墓碑，增量维护
    isize capacity;

    // 方法声明 —— 实现在文件末尾 (C API 之后)
    static xpHashMap make(xpAllocator a);
    void free();
    xpHashMap copy(xpAllocator a) const;
    V *insert(K key, V value);
    xpHashMapEntry<K, V> *get_entry(K key) const;
    V& get(K key);
    template<typename F> V& get_or_insert(K key, F&& factory);
    V& operator[](K key);
    b32 remove(K key);
    void clear();

    xpHashMapIterator<K, V> begin() const;
    xpHashMapIterator<K, V> end() const;
};

// --- 迭代器 ---

template<typename K, typename V>
struct xpHashMapIterator {
    const xpHashMapEntry<K, V>* entries;
    isize capacity;
    isize index;

    bool operator!=(const xpHashMapIterator& o) const { return index != o.index; }

    void operator++() {
        index++;
        while (index < capacity && entries[index].state != XP_HASH_SLOT_USED)
            index++;
    }

    const xpHashMapEntry<K, V>& operator*() const { return entries[index]; }
    const xpHashMapEntry<K, V>* operator->() const { return &entries[index]; }
};

// --- 内部: 重哈希 —— 只搬 USED 条目，墓碑自然消失 ---

template<typename K, typename V>
void xp_hash_map_rehash(xpHashMap<K, V> *map, xpHashMapEntry<K, V> *new_entries, isize new_capacity) {
    for (isize i = 0; i < new_capacity; ++i) {
        new_entries[i].state = XP_HASH_SLOT_EMPTY;
    }

    usize mask = static_cast<usize>(new_capacity) - 1;
    for (isize i = 0; i < map->capacity; ++i) {
        xpHashMapEntry<K, V> *old_entry = &map->entries[i];
        if (old_entry->state != XP_HASH_SLOT_USED) {
            continue;
        }

        usize index = std::hash<K>{}(old_entry->key) & mask;
        while (new_entries[index].state == XP_HASH_SLOT_USED) {
            index = (index + 1) & mask;
        }
        new (&new_entries[index].key) K(std::move(old_entry->key));
        new (&new_entries[index].value) V(std::move(old_entry->value));
        new_entries[index].state = XP_HASH_SLOT_USED;

        old_entry->key.~K();
        old_entry->value.~V();
        old_entry->state = XP_HASH_SLOT_EMPTY;
    }
}

// --- 内部: 扩容 ---

template<typename K, typename V>
void xp_hash_map_extend(xpHashMap<K, V> *map, isize new_capacity) {
    XP_ASSERT_MSG(new_capacity > map->capacity, "extend: new_capacity must be larger");

    if (map->entries == NULL) {
        map->entries = (xpHashMapEntry<K, V> *) xp_alloc(map->allocator, sizeof(xpHashMapEntry<K, V>) * new_capacity);
        for (isize i = 0; i < new_capacity; ++i) {
            map->entries[i].state = XP_HASH_SLOT_EMPTY;
        }
        map->capacity = new_capacity;
        map->occupied = 0;
        return;
    }

    xpHashMapEntry<K, V> *new_entries = (xpHashMapEntry<K, V> *) xp_alloc(map->allocator, sizeof(xpHashMapEntry<K, V>) * new_capacity);
    xp_hash_map_rehash(map, new_entries, new_capacity);
    xp_free(map->allocator, map->entries);
    map->entries = new_entries;
    map->capacity = new_capacity;
    map->occupied = map->count;   // 重建后只剩存活条目，墓碑全清
}

// --- 内部: 原地重建 —— 容量不变，只清墓碑 ---

// 墓碑不参与搬迁，故重建后 occupied == count。
template<typename K, typename V>
void xp_hash_map_rebuild(xpHashMap<K, V> *map) {
    if (map->capacity == 0 || map->entries == NULL) {
        return;
    }

    xpHashMapEntry<K, V> *new_entries = (xpHashMapEntry<K, V> *) xp_alloc(map->allocator, sizeof(xpHashMapEntry<K, V>) * map->capacity);
    xp_hash_map_rehash(map, new_entries, map->capacity);
    xp_free(map->allocator, map->entries);
    map->entries = new_entries;
    map->occupied = map->count;
}

// 已占槽位 = 存活 + 墓碑，由 insert/remove/rehash 增量维护

// --- 内部: 线性探测 ---

template<typename K, typename V>
LinearProbeResult xp_hash_map_linear_probe(const xpHashMap<K, V> *map, K key) {
    if (map->capacity == 0 || map->entries == nullptr) {
        return {};
    }

    usize hash_value = std::hash<K>{}(key);

    return linear_probe(hash_value, map->capacity, [&](isize index, bool* out_key_match) {
        const xpHashMapEntry<K, V>* entry = &map->entries[index];
        *out_key_match = false;
        if (entry->state == XP_HASH_SLOT_USED) {
            *out_key_match = (entry->key == key);
        }
        return entry->state;
    });
}

// === C API ===

template<typename K, typename V>
xpHashMap<K, V> xp_hash_map_make(xpAllocator allocator) {
    xpHashMap<K, V> map = {};
    map.allocator = allocator;
    return map;
}

template<typename K, typename V>
void xp_hash_map_free(xpHashMap<K, V> map) {
    if (map.entries != NULL) {
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
xpHashMap<K, V> xp_hash_map_copy(const xpHashMap<K, V> *o, xpAllocator allocator) {
    xpHashMap<K, V> copy = {};
    copy.allocator = allocator;
    copy.count = o->count;
    copy.occupied = o->occupied;
    copy.capacity = o->capacity;

    if (o->capacity > 0 && o->entries != NULL) {
        copy.entries = xp_alloc_array<xpHashMapEntry<K, V>>(allocator, copy.capacity);
        for (isize i = 0; i < copy.capacity; ++i) {
            copy.entries[i].state = o->entries[i].state;
            if (o->entries[i].state == XP_HASH_SLOT_USED) {
                new (&copy.entries[i].key) K(o->entries[i].key);
                new (&copy.entries[i].value) V(o->entries[i].value);
            }
        }
    }

    return copy;
}

template<typename K, typename V>
V *xp_hash_map_insert(xpHashMap<K, V> *map, K key, V value) {
    if (map->capacity == 0 || (double)map->count / (double)map->capacity >= 0.7) {
        isize new_cap = map->capacity == 0 ? 8 : map->capacity * 2;
        xp_hash_map_extend(map, new_cap);
    } else if ((double)map->occupied / (double)map->capacity >= 0.7) {
        // 墓碑挤占空槽：原地重建。只扩容的话，live 少而反复增删的表会无限涨容量
        xp_hash_map_rebuild(map);
    }

    auto probe_result = xp_hash_map_linear_probe(map, key);

    if (probe_result.found_index != -1) {
        map->entries[probe_result.found_index].value = value;
        return nullptr;
    }

    isize insert_index = -1;
    if (probe_result.first_tombstone != -1) {
        insert_index = probe_result.first_tombstone;
    } else if (probe_result.first_empty != -1) {
        insert_index = probe_result.first_empty;
    }

    XP_ASSERT_MSG(insert_index != -1, "insert: map is full after expansion");
    if (insert_index == -1) {
        return nullptr;
    }

    xpHashMapEntry<K, V>* insert_entry = &map->entries[insert_index];
    new (&insert_entry->key) K(key);
    new (&insert_entry->value) V(value);
    insert_entry->state = XP_HASH_SLOT_USED;

    map->count += 1;
    // 复用墓碑不增加占位，占空槽才增加
    if (probe_result.first_tombstone == -1) {
        map->occupied += 1;
    }
    return &insert_entry->value;
}

template<typename K, typename V>
xpHashMapEntry<K, V> *xp_hash_map_get_entry(xpHashMap<K, V> map, K key) {
    if (map.count == 0 || map.capacity == 0 || map.entries == nullptr) {
        return NULL;
    }

    auto probe_result = xp_hash_map_linear_probe(&map, key);
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
    xpHashMapEntry<K, V> *entry = xp_hash_map_get_entry(*map, key);
    if (entry != NULL) {
        entry->value = value;
        return &entry->value;
    }
    return NULL;
}

template<typename K, typename V>
b32 xp_hash_map_remove(xpHashMap<K, V> *map, K key) {
    xpHashMapEntry<K, V> *entry = xp_hash_map_get_entry(*map, key);
    if (entry != NULL) {
        entry->key.~K();
        entry->value.~V();
        entry->state = XP_HASH_SLOT_TOMBSTONE;
        map->count -= 1;
        return true;
    }
    return false;
}

template<typename K, typename V>
void xp_hash_map_clear(xpHashMap<K, V> *map) {
    if (map->entries == NULL) return;

    for (isize i = 0; i < map->capacity; ++i) {
        if (map->entries[i].state == XP_HASH_SLOT_USED) {
            map->entries[i].key.~K();
            map->entries[i].value.~V();
            map->entries[i].state = XP_HASH_SLOT_EMPTY;
        } else if (map->entries[i].state == XP_HASH_SLOT_TOMBSTONE) {
            map->entries[i].state = XP_HASH_SLOT_EMPTY;
        }
    }

    map->count = 0;
    map->occupied = 0;
}

// --- 方法实现 (调用上面的 C API) ---

template<typename K, typename V>
xpHashMap<K, V> xpHashMap<K, V>::make(xpAllocator a) {
    return xp_hash_map_make<K, V>(a);
}

template<typename K, typename V>
void xpHashMap<K, V>::free() {
    xp_hash_map_free(*this);
}

template<typename K, typename V>
xpHashMap<K, V> xpHashMap<K, V>::copy(xpAllocator a) const {
    return xp_hash_map_copy(this, a);
}

template<typename K, typename V>
V *xpHashMap<K, V>::insert(K key, V value) {
    return xp_hash_map_insert(this, key, value);
}

template<typename K, typename V>
xpHashMapEntry<K, V> *xpHashMap<K, V>::get_entry(K key) const {
    return xp_hash_map_get_entry(*this, key);
}

template<typename K, typename V>
V& xpHashMap<K, V>::get(K key) {
    return get_or_insert(key, []{ return V{}; });
}

template<typename K, typename V>
template<typename F>
V& xpHashMap<K, V>::get_or_insert(K key, F&& factory) {
    V* v = xp_hash_map_get(*this, key);
    if (v) return *v;
    return *insert(key, factory());
}

template<typename K, typename V>
V& xpHashMap<K, V>::operator[](K key) {
    V *v = xp_hash_map_get(*this, key);
    XP_ASSERT_MSG(v != nullptr, "operator[]: key not found");
    return *v;
}

template<typename K, typename V>
b32 xpHashMap<K, V>::remove(K key) {
    return xp_hash_map_remove(this, key);
}

template<typename K, typename V>
void xpHashMap<K, V>::clear() {
    xp_hash_map_clear(this);
}

template<typename K, typename V>
xpHashMapIterator<K, V> xpHashMap<K, V>::begin() const {
    isize idx = 0;
    while (idx < capacity && entries[idx].state != XP_HASH_SLOT_USED)
        idx++;
    return {entries, capacity, idx};
}

template<typename K, typename V>
xpHashMapIterator<K, V> xpHashMap<K, V>::end() const {
    return {entries, capacity, capacity};
}

#endif // __cplusplus

#endif // XOAOP_HASHMAP_H
