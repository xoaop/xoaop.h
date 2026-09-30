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
    xpHashSlotState state;
    K key;
};

template<typename K>
struct xpHashSetIterator;

template<typename K>
struct xpHashSet {
    xpAllocator allocator;
    xpHashSetEntry<K> *entries;
    isize count;
    isize occupied;   // 已占槽位 = 存活 + 墓碑，增量维护
    isize capacity;

    // 方法声明 —— 实现在文件末尾 (C API 之后)
    static xpHashSet make(xpAllocator a);
    void free();
    xpHashSet copy(xpAllocator a) const;
    K *insert(K key);
    b32 contains(K key) const;
    const K& operator[](K key) const;
    b32 remove(K key);
    void clear();

    xpHashSetIterator<K> begin() const;
    xpHashSetIterator<K> end() const;
};

// --- 迭代器 ---

template<typename K>
struct xpHashSetIterator {
    const xpHashSetEntry<K>* entries;
    isize capacity;
    isize index;

    bool operator!=(const xpHashSetIterator& o) const { return index != o.index; }

    void operator++() {
        index++;
        while (index < capacity && entries[index].state != XP_HASH_SLOT_USED)
            index++;
    }

    const xpHashSetEntry<K>& operator*() const { return entries[index]; }
    const xpHashSetEntry<K>* operator->() const { return &entries[index]; }
};

// --- 内部: 线性探测 ---

template<typename K>
LinearProbeResult xp_hash_set_linear_probe(const xpHashSet<K> *set, K key) {
    if (set->capacity == 0 || set->entries == nullptr) {
        return {};
    }

    usize hash_value = std::hash<K>{}(key);
    return linear_probe(hash_value, set->capacity, [&](isize index, bool* out_key_match) {
        const xpHashSetEntry<K>* entry = &set->entries[index];
        *out_key_match = false;
        if (entry->state == XP_HASH_SLOT_USED) {
            *out_key_match = (entry->key == key);
        }
        return entry->state;
    });
}

// --- 内部: 扩容 ---

// --- 内部: 重哈希 —— 只搬 USED 条目，墓碑自然消失 ---

template<typename K>
void xp_hash_set_rehash(xpHashSet<K> *set, xpHashSetEntry<K> *new_entries, isize new_capacity) {
    for (isize i = 0; i < new_capacity; ++i) {
        new_entries[i].state = XP_HASH_SLOT_EMPTY;
    }

    usize mask = static_cast<usize>(new_capacity) - 1;
    for (isize i = 0; i < set->capacity; ++i) {
        xpHashSetEntry<K> *old_entry = &set->entries[i];
        if (old_entry->state != XP_HASH_SLOT_USED) {
            continue;
        }

        usize index = std::hash<K>{}(old_entry->key) & mask;
        while (new_entries[index].state == XP_HASH_SLOT_USED) {
            index = (index + 1) & mask;
        }
        new (&new_entries[index].key) K(std::move(old_entry->key));
        new_entries[index].state = XP_HASH_SLOT_USED;

        old_entry->key.~K();
        old_entry->state = XP_HASH_SLOT_EMPTY;
    }
}

// --- 内部: 扩容 ---

template<typename K>
void xp_hash_set_extend(xpHashSet<K> *set, isize new_capacity) {
    XP_ASSERT_MSG(new_capacity > set->capacity, "extend: new_capacity must be larger");

    if (set->entries == NULL) {
        set->entries = (xpHashSetEntry<K> *) xp_alloc(set->allocator, sizeof(xpHashSetEntry<K>) * new_capacity);
        for (isize i = 0; i < new_capacity; ++i) {
            set->entries[i].state = XP_HASH_SLOT_EMPTY;
        }
        set->capacity = new_capacity;
        set->occupied = 0;
        return;
    }

    xpHashSetEntry<K> *new_entries = (xpHashSetEntry<K> *) xp_alloc(set->allocator, sizeof(xpHashSetEntry<K>) * new_capacity);
    xp_hash_set_rehash(set, new_entries, new_capacity);
    xp_free(set->allocator, set->entries);
    set->entries = new_entries;
    set->capacity = new_capacity;
    set->occupied = set->count;   // 重建后只剩存活条目，墓碑全清
}

// --- 内部: 原地重建 —— 容量不变，只清墓碑 ---

// 墓碑不参与搬迁，故重建后 occupied == count。
template<typename K>
void xp_hash_set_rebuild(xpHashSet<K> *set) {
    if (set->capacity == 0 || set->entries == NULL) {
        return;
    }

    xpHashSetEntry<K> *new_entries = (xpHashSetEntry<K> *) xp_alloc(set->allocator, sizeof(xpHashSetEntry<K>) * set->capacity);
    xp_hash_set_rehash(set, new_entries, set->capacity);
    xp_free(set->allocator, set->entries);
    set->entries = new_entries;
    set->occupied = set->count;
}

// 已占槽位 = 存活 + 墓碑，由 insert/remove/rehash 增量维护

// === C API ===

template<typename K>
xpHashSet<K> xp_hash_set_make(xpAllocator allocator) {
    xpHashSet<K> hash_set = {};
    hash_set.allocator = allocator;
    return hash_set;
}

template<typename K>
void xp_hash_set_free(xpHashSet<K> set) {
    if (set.entries != NULL) {
        for (isize i = 0; i < set.capacity; ++i) {
            if (set.entries[i].state == XP_HASH_SLOT_USED) {
                set.entries[i].key.~K();
            }
        }
        xp_free(set.allocator, set.entries);
    }
}

template<typename K>
xpHashSet<K> xp_hash_set_copy(xpHashSet<K> *set, xpAllocator allocator) {
    xpHashSet<K> copy = {};
    copy.allocator = allocator;
    copy.count = set->count;
    copy.occupied = set->occupied;
    copy.capacity = set->capacity;

    if (set->capacity > 0 && set->entries != NULL) {
        copy.entries = xp_alloc_array<xpHashSetEntry<K>>(allocator, copy.capacity);
        for (isize i = 0; i < copy.capacity; ++i) {
            copy.entries[i].state = XP_HASH_SLOT_EMPTY;
        }

        for (isize i = 0; i < set->capacity; ++i) {
            const xpHashSetEntry<K> *src_entry = &set->entries[i];
            xpHashSetEntry<K> *dst_entry = &copy.entries[i];

            dst_entry->state = src_entry->state;
            if (src_entry->state == XP_HASH_SLOT_USED) {
                new (&dst_entry->key) K(src_entry->key);
            }
        }
    }

    return copy;
}

template<typename K>
K *xp_hash_set_insert(xpHashSet<K> *set, K key) {
    if (set->capacity == 0 || (double)set->count / (double)set->capacity >= 0.7) {
        isize new_cap = set->capacity == 0 ? 8 : set->capacity * 2;
        xp_hash_set_extend(set, new_cap);
    } else if ((double)set->occupied / (double)set->capacity >= 0.7) {
        // 墓碑挤占空槽：原地重建。只扩容的话，live 少而反复增删的表会无限涨容量
        xp_hash_set_rebuild(set);
    }

    auto probe_result = xp_hash_set_linear_probe(set, key);

    if (probe_result.found_index != -1) {
        return nullptr;
    }

    isize insert_index = -1;
    if (probe_result.first_tombstone != -1) {
        insert_index = probe_result.first_tombstone;
    } else if (probe_result.first_empty != -1) {
        insert_index = probe_result.first_empty;
    }

    XP_ASSERT_MSG(insert_index != -1, "insert: set is full after expansion");
    if (insert_index == -1) {
        return nullptr;
    }

    xpHashSetEntry<K>* insert_entry = &set->entries[insert_index];
    new (&insert_entry->key) K(key);
    insert_entry->state = XP_HASH_SLOT_USED;

    set->count += 1;
    // 复用墓碑不增加占位，占空槽才增加
    if (probe_result.first_tombstone == -1) {
        set->occupied += 1;
    }
    return &insert_entry->key;
}

template<typename K>
xpHashSetEntry<K> *xp_hash_set_get_entry(xpHashSet<K> *set, K key) {
    if (set->count == 0 || set->capacity == 0 || set->entries == nullptr) {
        return NULL;
    }

    auto probe_result = xp_hash_set_linear_probe(set, key);
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
    return xp_hash_set_get_entry(set, key) != NULL;
}

template<typename K>
b32 xp_hash_set_remove(xpHashSet<K> *set, K key) {
    xpHashSetEntry<K> *entry = xp_hash_set_get_entry(set, key);
    if (entry != NULL) {
        entry->key.~K();
        entry->state = XP_HASH_SLOT_TOMBSTONE;
        set->count -= 1;
        return true;
    }
    return false;
}

template<typename K>
void xp_hash_set_clear(xpHashSet<K> *set) {
    if (set->entries == NULL || set->capacity <= 0) {
        set->count = 0;
        set->occupied = 0;
        return;
    }

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
    set->occupied = 0;
}

// --- 方法实现 (调用上面的 C API) ---

template<typename K>
xpHashSet<K> xpHashSet<K>::make(xpAllocator a) {
    return xp_hash_set_make<K>(a);
}

template<typename K>
void xpHashSet<K>::free() {
    xp_hash_set_free(*this);
}

template<typename K>
xpHashSet<K> xpHashSet<K>::copy(xpAllocator a) const {
    return xp_hash_set_copy(this, a);
}

template<typename K>
K *xpHashSet<K>::insert(K key) {
    return xp_hash_set_insert(this, key);
}

template<typename K>
b32 xpHashSet<K>::contains(K key) const {
    return xp_hash_set_find(const_cast<xpHashSet<K>*>(this), key);
}

template<typename K>
const K& xpHashSet<K>::operator[](K key) const {
    K *v = xp_hash_set_get(const_cast<xpHashSet<K>*>(this), key);
    XP_ASSERT_MSG(v != nullptr, "operator[]: key not found in set");
    return *v;
}

template<typename K>
b32 xpHashSet<K>::remove(K key) {
    return xp_hash_set_remove(this, key);
}

template<typename K>
void xpHashSet<K>::clear() {
    xp_hash_set_clear(this);
}

template<typename K>
xpHashSetIterator<K> xpHashSet<K>::begin() const {
    isize idx = 0;
    while (idx < capacity && entries[idx].state != XP_HASH_SLOT_USED)
        idx++;
    return {entries, capacity, idx};
}

template<typename K>
xpHashSetIterator<K> xpHashSet<K>::end() const {
    return {entries, capacity, capacity};
}

#endif // __cplusplus

#endif // XOAOP_HASHSET_H
