/*
    Файл реализации хэш-мультиотображения c_hash_multimap
    Разработка, отладка и сборка производилась в:
    ОС: Windows 10/x64
    IDE: Code::Blocks 17.12
    Компилятор: default Code::Blocks 17.12 MinGW
    Разработчик: Глухманюк Максим
    Эл. почта: mgneo@yandex.ru
    Место: Российская Федерация, Самарская область, Сызрань
    Дата: 11.04.2018
    Лицензия: GPLv3
*/

#ifndef C_HASH_MULTIMAP_H
#define C_HASH_MULTIMAP_H

#include <stddef.h>

typedef struct s_c_hash_multimap c_hash_multimap;

c_hash_multimap *c_hash_multimap_create(size_t (*const _hash_key)(const void *const _key),
                                        size_t (*const _comp_key)(const void *const _key_a,
                                                                  const void *const _key_b),
                                        size_t (*const _comp_data)(const void *const _data_a,
                                                                   const void *const _data_b),
                                        const size_t _slots_count,
                                        const float _max_load_factor,
                                        size_t *const _error);

ptrdiff_t c_hash_multimap_delete(c_hash_multimap *const _hash_multimap,
                                 void (*const _del_key)(void *const _key),
                                 void (*const _del_data)(void *const _data));

ptrdiff_t c_hash_multimap_clear(c_hash_multimap *const _hash_multimap,
                                void (*const _del_key)(void *const _key),
                                void (*const _del_data)(void *const _data));

ptrdiff_t c_hash_multimap_resize(c_hash_multimap *const _hash_multimap,
                                 const size_t _slots_count);

ptrdiff_t c_hash_multimap_insert(c_hash_multimap *const _hash_multimap,
                                 const void *const _key,
                                 const void *const _data);

ptrdiff_t c_hash_multimap_erase(c_hash_multimap *const _hash_multimap,
                                const void *const _key,
                                const void *const _data,
                                void (*const _del_key)(void *const _key),
                                void (*const _del_data)(void *const _data));

size_t c_hash_multimap_erase_all(c_hash_multimap *const _hash_multimap,
                                 const void *const _key,
                                 void (*const _del_key)(void *const _key),
                                 void (*const _del_data)(void *const _data),
                                 size_t *const _error);

ptrdiff_t c_hash_multimap_for_each(c_hash_multimap *const _hash_multimap,
                                   void (*const _action_key)(const void *const _key),
                                   void (*const _action_data)(void *const _data));

ptrdiff_t c_hash_multimap_key_check(const c_hash_multimap *const _hash_multimap,
                                    const void *const _key);

size_t c_hash_multimap_key_count(const c_hash_multimap *const _hash_multimap,
                                 const void *const _key,
                                 size_t *const _error);

ptrdiff_t c_hash_multimap_pair_check(const c_hash_multimap *const _hash_multimap,
                                     const void *const _key,
                                     const void *const _data);

size_t c_hash_multimap_pair_count(const c_hash_multimap *const _hash_multimap,
                                  const void *const _key,
                                  const void *const _data,
                                  size_t *const _error);

void** c_hash_multimap_datas(c_hash_multimap *const _hash_multimap,
                             const void *const _key,
                             size_t *const _error);

size_t c_hash_multimap_slots_count(const c_hash_multimap *const _hash_multimap,
                                   size_t *const _error);

size_t c_hash_multimap_unique_keys_count(const c_hash_multimap *const _hash_multimap,
                                         size_t *const _error);

size_t c_hash_multimap_pairs_count(const c_hash_multimap *const _hash_multimap,
                                   size_t *const _error);

#endif
