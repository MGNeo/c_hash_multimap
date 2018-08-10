﻿#ifndef C_HASH_MULTIMAP_H
#define C_HASH_MULTIMAP_H

#include <stddef.h>

typedef struct s_c_hash_multimap c_hash_multimap;

c_hash_multimap *c_hash_multimap_create(size_t (*const _hash_key)(const void *const _key),
                                        size_t (*const _hash_data)(const void *const _data),
                                        size_t (*const _comp_key)(const void *const _key_a,
                                                                  const void *const _key_b),
                                        size_t (*const _comp_data)(const void *const _data_a,
                                                                   const void *const _data_b),
                                        const size_t _slots_count,
                                        const float _max_load_factor);

ptrdiff_t c_hash_multimap_delete(c_hash_multimap *const _hash_multimap,
                                 void (*const _del_key)(void *const _key),
                                 void (*const _del_data)(void *const _data));

/*ptrdiff_t c_hash_multimap_insert(c_hash_multimap *const _hash_multimap,
                                 const void *const _key,
                                 const void *const _data);*/

ptrdiff_t c_hash_multimap_resize(c_hash_multimap *const _hash_multimap,
                                 const size_t _slots_count);

ptrdiff_t c_hash_multimap_clear(c_hash_multimap *const _hash_multimap,
                                void (*const _del_key)(void *const _key),
                                void (*const _del_data)(void *const _data));

ptrdiff_t c_hash_multimap_erase(c_hash_multimap *const _hash_multimap,
                                const void *const _key,
                                const void *const _data,
                                void (*const _del_key)(void *const _key),
                                void (*const _del_data)(void *const _data));

ptrdiff_t c_hash_multimap_erase_all(c_hash_multimap *const _hash_multimap,
                                    const void *const _key,
                                    void (*const _del_key)(void *const _key),
                                    void (*const _del_data)(void *const _data));

/*ptrdiff_t c_hash_multimap_check_key(const c_hash_multimap *const _hash_multimap,
                                    const void *const _key);

ptrdiff_t c_hash_multimap_check(const c_hash_multimap *const _hash_map,
                                const void *const _key,
                                const void *const _data);

ptrdiff_t c_hash_multimap_check_data(const c_hash_multimap *const _hash_multimap,
                                     const void *const _data);

ptrdiff_t c_hash_multimap_for_each(c_hash_multimap *const _hash_multimap,
                                   void (*const _action_key)(const void *const _key),
                                   void (*const _action_data)(const void *const _data));

size_t c_hash_multimap_count(const c_hash_multimap *const _hash_multimap,
                             const void *const _key,
                             const void *const _data);

size_t c_hash_multimap_count_key(const c_hash_multimap *const _hash_multimap,
                                 const void *const _key);

size_t c_hash_multimap_count_data(const c_hash_multimap *const _hash_multimap,
                                  const void *const _data);

void **c_hash_multimap_at(c_hash_multimap *const _hash_multimap,
                          const void *const _key);

size_t c_hash_multimap_slots_count(const c_hash_multimap *const _hash_multimap);

size_t c_hash_multimap_keys_count(const c_hash_multimap *const _hash_multimap);

size_t c_hash_multimap_datas_count(const c_hash_multimap *const _hash_multimap);

float c_hash_multimap_max_load_factor(const c_hash_multimap *const _hash_multimap);*/

#endif
