#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <memory.h>

#include "c_hash_multimap.h"

// Количество слотов, задаваемое хэш-мультиотображению с нулем слотов при автоматическом
// расширении.
#define C_HASH_MULTIMAP_0 ( (size_t) 1024 )

// Минимально возможное значение max_load_factor.
#define C_HASH_MULTIMAP_MLF_MIN ( (float) 0.01f )

// Максимально возможное значение max_load_factor.
#define C_HASH_MULTIMAP_MLF_MAX ( (float) 1.f )

typedef struct s_c_hash_multimap_node c_hash_multimap_node;

typedef struct s_c_hash_multimap_chain c_hash_multimap_chain;

struct s_c_hash_multimap_node
{
    c_hash_multimap_node *next_node;

    void *key;
    void *data;

    size_t data_hash;
};

// Цепочка нужна для оптимизации расхода памяти и количества операций.
struct s_c_hash_multimap_chain
{
    c_hash_multimap_chain *next_chain;
    c_hash_multimap_node *head;

    size_t key_hash,
           nodes_count;
};

struct s_c_hash_multimap
{
    // Функция генерации хэша по ключу.
    size_t (*hash_key)(const void *const _key);
    // Функция генерации хэша по данным.
    size_t (*hash_data)(const void *const _data);

    // Функция детального сравнения ключей.
    // В случае идентичности ключей должна возвращать > 0.
    size_t (*comp_key)(const void *const _key_a,
                       const void *const _key_b);
    // Функция детального сравнения данных.
    // В случае идентичности данных должна возвращать > 0.
    size_t (*comp_data)(const void *const _data_a,
                        const void *const _data_b);

    size_t slots_count,
           chains_count,
           nodes_count;

    float max_load_factor;

    c_hash_multimap_chain **slots;
};

// Создание хэш-мультиотображения.
// Позволяет создавать хэш-мультиотображение с нулем слотов.
// В случае успеха возвращает указатель на созданное хэш-мультиотображение.
// В случае ошибки возвращает NULL.
c_hash_multimap *c_hash_multimap_create(size_t (*const _hash_key)(const void *const _key),
                                        size_t (*const _hash_data)(const void *const _data),
                                        size_t (*const _comp_key)(const void *const _key_a,
                                                                  const void *const _key_b),
                                        size_t (*const _comp_data)(const void *const _data_a,
                                                                   const void *const _data_b),
                                        const size_t _slots_count,
                                        const float _max_load_factor)
{
    if (_hash_key == NULL) return NULL;
    if (_hash_data == NULL) return NULL;

    if (_comp_key == NULL) return NULL;
    if (_comp_data == NULL) return NULL;

    if ( (_max_load_factor < C_HASH_MULTIMAP_MLF_MIN) ||
         (_max_load_factor > C_HASH_MULTIMAP_MLF_MAX) )
    {
        return NULL;
    }

    c_hash_multimap_chain **new_slots = NULL;

    if (_slots_count > 0)
    {
        const size_t new_slots_size = _slots_count * sizeof(c_hash_multimap_chain*);
        if ( (new_slots_size == 0) ||
             (new_slots_size / _slots_count != sizeof(c_hash_multimap_chain*)) )
        {
            return NULL;
        }

        new_slots = (c_hash_multimap_chain**)malloc(new_slots_size);
        if (new_slots == NULL)
        {
            return NULL;
        }

        memset(new_slots, 0, new_slots_size);
    }

    c_hash_multimap *const new_hash_multimap = (c_hash_multimap*)malloc(sizeof(c_hash_multimap));
    if (new_hash_multimap == NULL)
    {
        free(new_slots);
        return NULL;
    }

    new_hash_multimap->hash_key = _hash_key;
    new_hash_multimap->hash_data = _hash_data;
    new_hash_multimap->comp_key = _comp_key;
    new_hash_multimap->comp_data = _comp_data;

    new_hash_multimap->slots_count = _slots_count;
    new_hash_multimap->chains_count = 0;
    new_hash_multimap->nodes_count = 0;

    new_hash_multimap->max_load_factor = _max_load_factor;

    new_hash_multimap->slots = new_slots;

    return new_hash_multimap;
}

// Удаление хэш-мультиотображения.
// В случае успеха возвращает > 0, иначе < 0.
ptrdiff_t c_hash_multimap_delete(c_hash_multimap *const _hash_multimap,
                                 void (*const _del_key)(void *const _key),
                                 void (*const _del_data)(void *const _data))
{
    if (c_hash_multimap_clear(_hash_multimap, _del_key, _del_data) < 0)
    {
        return -1;
    }

    free(_hash_multimap->slots);

    free(_hash_multimap);

    return 1;
}

// Очищает хэш-мультимножество ото всех элементов, при этом слоты остаются существовать в прежнем количестве.
// В случае успешного очищения возвращает > 0.
// Если очищать нечего, возвращает 0.
// В случае ошибки возвращает < 0.
ptrdiff_t c_hash_multimap_clear(c_hash_multimap *const _hash_multimap,
                                void (*const _del_key)(void *const _key),
                                void (*const _del_data)(void *const _data))
{
    if (_hash_multimap == NULL) return -1;

    if (_hash_multimap->chains_count == 0) return 0;

    size_t count = _hash_multimap->chains_count;

    // Идем по всем слотам.
    for (size_t s = 0; (s < _hash_multimap->slots_count)&&(count > 0); ++s)
    {
        // Если слот непустой.
        if (_hash_multimap->slots[s] != NULL)
        {
            // Перебираем все цепочки слота.
            c_hash_multimap_chain *select_chain = _hash_multimap->slots[s],
                                      *delete_chain;
            while (select_chain != NULL)
            {
                delete_chain = select_chain;
                select_chain = select_chain->next_chain;

                // Перебираем все узлы цепочки.
                c_hash_multimap_node *select_node = delete_chain->head,
                                         *delete_node;
                while (select_node != NULL)
                {
                    delete_node = select_node;
                    select_node = select_node->next_node;

                    // Удаляем ключ узла.
                    if (_del_key != NULL)
                    {
                        _del_key(delete_node->key);
                    }
                    // Удаляем данные узла.
                    if (_del_data != NULL)
                    {
                        _del_data(delete_node->data);
                    }
                    // Удаляем узел.
                    free(delete_node);
                }
                // Удаляем цепочку.
                free(delete_chain);

                --count;
            }

            _hash_multimap->slots[s] = NULL;
        }
    }

    _hash_multimap->chains_count = 0;
    _hash_multimap->nodes_count = 0;

    return 1;
}

// Вставка в хэш-мультиотображение заданных данных по заданному ключу.
// В случае успеха возвращает > 0, ключ и данные захватываются хэш-мультиотображением.
// В случае ошибки возвращает < 0, ключ и данные не захватываются хэш-мультиотображением.
ptrdiff_t c_hash_multimap_insert(c_hash_multimap *const _hash_multimap,
                                 const void *const _key,
                                 const void *const _data)
{
    if (_hash_multimap == NULL) return -1;
    if (_key == NULL) return -2;
    if (_data == NULL) return -3;

    // Первым делом контролируем процесс увеличения количества слотов.

    // Если слотов вообще нет.
    if (_hash_multimap->slots_count == 0)
    {
        if (c_hash_multimap_resize(_hash_multimap, C_HASH_MULTIMAP_0) <= 0)
        {
            return -4;
        }
    } else {
        // Если слоты есть, то при достижении предела загруженности увеличиваем количество слотов.
        const float load_factor = (float)_hash_multimap->chains_count / _hash_multimap->slots_count;
        if (load_factor >= _hash_multimap->max_load_factor)
        {
            // Определим новое количество слотов.
            size_t new_slots_count = (size_t)(_hash_multimap->slots_count * 1.75f);
            if ( (new_slots_count == 0) ||
                 (new_slots_count < _hash_multimap->slots_count) )
            {
                return -5;
            }
            new_slots_count += 1;
            if (new_slots_count == 0)
            {
                return -6;
            }

            // Пытаемся расширить слоты.
            if (c_hash_multimap_resize(_hash_multimap, new_slots_count) < 0)
            {
                return -7;
            }
        }
    }

    // Встраиваем данные в хэш-мультиотображение.

    // Неприведенный хэш ключа.
    const size_t key_hash = _hash_multimap->hash_key(_key);

    // Приведенный хэш.
    const size_t presented_key_hash = key_hash % _hash_multimap->slots_count;

    // Проходим по всем цепочкам слота, пытаясь найти, в какой из них хранятся данные с таким же ключом.
    c_hash_multimap_chain *select_chain = _hash_multimap->slots[presented_key_hash];
    while (select_chain != NULL)
    {
        if (key_hash == select_chain->key_hash)
        {
            if (_hash_multimap->comp_key(_key, select_chain->key_hash) > 0)
            {
                break;
            }
        }
        select_chain = select_chain->next_chain;
    }

    // Если цепочки не существует, создадим ее.
    size_t created = 0;
    if (select_chain == NULL)
    {
        created = 1;
        // Попытаемся выделить память под цепочку.
        c_hash_multimap_chain *const new_chain = (c_hash_multimap_chain*)malloc(sizeof(c_hash_multimap_chain));
        if (new_chain == NULL)
        {
            return -8;
        }

        // Встраиваем цепочку в слот, делаем это сразу, чтобы дальнейший алгоритм вставки нового узла
        // был описан единообразно как для уже существовавших цепочек, так и для вновь созданных.
        new_chain->next_chain = _hash_multimap->slots[presented_key_hash];
        _hash_multimap->slots[presented_key_hash] = new_chain;

        // Установим параметры цепочки.
        new_chain->head = NULL;
        new_chain->nodes_count = 0;

        // Уникальных ключей в хэш-мультиотображении стало больше.
        ++_hash_multimap->chains_count;

        // Выделяем созданную цепочку, вставлять данные будем в нее.
        select_chain = new_chain;
    }

    // Создадим узел и вставим его в цепочку.
    // Если вставка не удалась, и если мы создавали цепочку, удаляем ее,
    // потому что пустая цепочка существовать не должна.

    // Пытаемся выделить память под узел.
    c_hash_multimap_node *new_node = (c_hash_multimap_node*)malloc(sizeof(c_hash_multimap_node));
    // Если память выделить не удалось.
    if (new_node == NULL)
    {
        // Если цепочка была создана.
        if (created == 1)
        {
            // Вырезаем созданную цепочку из слота.
            _hash_multimap->slots[presented_key_hash] = select_chain->next_chain;
            // Освобождаем память из-под созданной цепочки.
            free(select_chain);
        }

        --_hash_multimap->chains_count;
        return -9;
    }

    // Вставляем узел в цепочку.
    new_node->next_node = select_chain->head;
    select_chain->head = new_node;

    // Узел захватывает ключ и данные.
    new_node->key = (void*)_key;
    new_node->data = (void*)_data;

    // Записываем в узел неприведенный хэш данных этого узла.
    new_node->data_hash = _hash_multimap->hash_data(_data);

    // Если цепочка была создана, записываем в нее неприведенный хэш ключа.
    if (created == 1)
    {
        select_chain->key_hash = _hash_multimap->hash_key(_key);
    }

    // Узлов в цепочке стало больше.
    ++select_chain->nodes_count;

    // Узлов в хэш-мультиотображении стало больше.
    ++_hash_multimap->nodes_count;

    return 1;
}

// Задает хэш-мультиотображению новое количество слотов.
// Позволяет расширять хэш-мультиотображение с нулем слотов.
// Если в хэш-мультиотображении есть данные, то попытка задать нулевое
// количество слотов считается ошибкой.
// Если хэш-мультиотображение перестраивается, возвращает > 0.
// Если не перестраивается, возвращает 0.
// В случае ошибки возвращает < 0.
ptrdiff_t c_hash_multimap_resize(c_hash_multimap *const _hash_multimap,
                                 const size_t _slots_count)
{
    if (_hash_multimap == NULL) return -1;

    if (_slots_count == _hash_multimap->slots_count) return 0;

    if (_slots_count == 0)
    {
        if (_hash_multimap->chains_count != 0)
        {
            return -2;
        }

        free(_hash_multimap->slots);
        _hash_multimap->slots = 0;

        _hash_multimap->slots_count = 0;

        return 1;
    } else {
        const size_t new_slots_size = _slots_count * sizeof(c_hash_multimap_chain*);
        if ( (new_slots_size == 0) ||
             (new_slots_size / _slots_count != sizeof(c_hash_multimap_chain*)) )
        {
            return -3;
        }

        c_hash_multimap_chain **const new_slots = (c_hash_multimap_chain**)malloc(new_slots_size);
        if (new_slots == NULL)
        {
            return -4;
        }

        memset(new_slots, 0, new_slots_size);

        // Если есть паки, которые необходимо перенести.
        if (_hash_multimap->chains_count > 0)
        {
            size_t count = _hash_multimap->chains_count;
            // Проходим по всем слотам.
            for (size_t s = 0; (s < _hash_multimap->slots_count)&&(count > 0); ++s)
            {
                // Если в слоте есть цепочки.
                if (_hash_multimap->slots[s] != NULL)
                {
                    // Проходим по всем цепочкам слота.
                    c_hash_multimap_chain *select_chain = _hash_multimap->slots[s],
                                          *relocate_chain;
                    while (select_chain != NULL)
                    {
                        relocate_chaint = select_pack;
                        select_pack = select_pack->next_pack;

                        // Хэш пака, приведенный к новому количеству слотов
                        const size_t presented_hash_key = relocate_pack->hash % _slots_count;

                        // Перенос пака.
                        relocate_pack->next_pack = new_slots[presented_hash_key];
                        new_slots[presented_hash_key] = relocate_pack;

                        --count;
                    }
                }
            }

        }

        free(_hash_multimap->slots);

        // Используем новые слоты.
        _hash_multimap->slots = new_slots;
        _hash_multimap->slots_count = _slots_count;

        return 2;
    }
}
/*
// Удаляет из хэш-мультиотображения данные, связанные с заданным ключом,
// та так же ключ, если с ключом больше не связано никаких данных.
// В случае успешного удаления возвращает > 0.
// Если указанных данных не существует, возвращает 0.
// В случае ошибки возвращает < 0.
ptrdiff_t c_hash_multimap_erase(c_hash_multimap *const _hash_multimap,
                                const void *const _key,
                                const void *const _data,
                                void (*const _del_key)(void *const _key),
                                void (*const _del_data)(void *const _data))
{
    if (_hash_multimap == NULL) return -1;
    if (_key == NULL) return -2;
    if (_data == NULL) return -3;

    if (_hash_multimap->keys_count == 0) return -4;

    // Неприведенный хэш ключа.
    const size_t hash = _hash_multimap->hash_key(_key);

    // Приведенный хэш ключа.
    const size_t presented_hash = hash % _hash_multimap->slots_count;

    // Поиск пака с заданным ключом.
    c_hash_multimap_pack *select_pack = _hash_multimap->slots[presented_hash],
                         *prev_pack = NULL;
    while (select_pack != NULL)
    {
        if (hash == select_pack->hash)
        {
            if (_hash_multimap->comp_key(_key, select_pack->key) > 0)
            {
                if (c_hash_multiset_erase(select_pack->data, _data, _del_data) > 0)
                {
                    --_hash_multimap->datas_count;

                    // Если данные пака опустели, необходимо пак удалить.
                    if (c_hash_multiset_uniques_count(select_pack->data) == 0)
                    {
                        c_hash_multiset_delete(select_pack->data, _del_data);
                        if (_del_key != NULL)
                        {
                            _del_key( select_pack->key );
                        }

                        // Сшиваем разрыв.
                        if (prev_pack != NULL)
                        {
                            prev_pack->next_pack = select_pack->next_pack;
                        } else {
                            _hash_multimap->slots[presented_hash] = select_pack->next_pack;
                        }

                        free(select_pack);

                        --_hash_multimap->keys_count;
                    }

                    return 1;
                } else {
                    return 0;
                }
            }
        }
        prev_pack = select_pack;
        select_pack = select_pack->next_pack;
    }

    return 0;
}

// Удаляет из хэш-мультиотображения все данные, связанные с данным ключом (и ключ).
// В случае успешного удаления возвращает > 0.
// Если в хэш-мультиотображении нет заданного ключа, возвращает 0.
// В случае ошибки возвращает < 0.
ptrdiff_t c_hash_multimap_erase_all(c_hash_multimap *const _hash_multimap,
                                    const void *const _key,
                                    void (*const _del_key)(void *const _key),
                                    void (*const _del_data)(void *const _data))
{
    if (_hash_multimap == NULL) return -1;
    if (_key == NULL) return -2;

    if (_hash_multimap->keys_count == 0) return 0;

    // Неприведенный хэш искомого ключа.
    const size_t hash = _hash_multimap->hash_key(_key);

    // Приведенный хэш заданного ключа.
    const size_t presented_hash = hash % _hash_multimap->slots_count;

    if (_hash_multimap->slots[presented_hash] != NULL)
    {
        c_hash_multimap_pack *select_pack = _hash_multimap->slots[presented_hash],
                             *prev_pack = NULL;
        while (select_pack != NULL)
        {
            if (hash == select_pack->hash)
            {
                if (_hash_multimap->comp_key(_key, select_pack->key) > 0)
                {
                    --_hash_multimap->keys_count;
                    _hash_multimap->datas_count -= c_hash_multiset_nodes_count(select_pack->data);

                    // Удаляем множество данных пака.
                    c_hash_multiset_delete(select_pack->data, _del_data);

                    // Удаляем ключ.
                    if (_del_key != NULL)
                    {
                        _del_key(select_pack->key);
                    }

                    // Сшиваем разрыв.
                    if (prev_pack != NULL)
                    {
                        prev_pack->next_pack = select_pack->next_pack;
                    } else {
                        _hash_multimap->slots[presented_hash] = select_pack->next_pack;
                    }

                    free(select_pack);

                    return 1;
                }
            }

            prev_pack = select_pack;
            select_pack = select_pack->next_pack;
        }
    }

    return 0;
}

// Проверка наличия в хэш-мультиотображении заданного ключа.
// В случае наличия ключа возвращает > 0.
// Если ключа нет, то возвращает 0.
// В случае ошибки возвращает < 0.
ptrdiff_t c_hash_multimap_check_key(const c_hash_multimap *const _hash_multimap,
                                    const void *const _key)
{
    if (_hash_multimap == NULL) return -1;
    if (_key == NULL) return -2;

    if (_hash_multimap->keys_count == 0) return 0;

    // Неприведенный хэш ключа.
    const size_t hash = _hash_multimap->hash_key(_key);

    // Приведенный хэш.
    const size_t presented_hash = hash % _hash_multimap->slots_count;

    if (_hash_multimap->slots[presented_hash] == NULL)
    {
        const c_hash_multimap_pack *select_pack = _hash_multimap->slots[presented_hash];
        while (select_pack != NULL)
        {
            if (hash == select_pack->hash)
            {
                if (_hash_multimap->comp_key(_key, select_pack->key) > 0)
                {
                    return 1;
                }
            }

            select_pack = select_pack->next_pack;
        }
    }

    return 0;
}

// Проверка наличия в хэш-мультимножестве заданных данных по заданному ключу.
// Если данные по заданному ключу есть, возвращает > 0.
// Если данных нет, возвращает 0.
// В случае ошибки возвращает < 0.
ptrdiff_t c_hash_multimap_check(const c_hash_multimap *const _hash_multimap,
                                const void *const _key,
                                const void *const _data)
{
    if (_hash_multimap == NULL) return -1;
    if (_key == NULL) return -2;
    if (_data == NULL) return -3;

    if (_hash_multimap->keys_count == 0) return 0;

    // Неприведенный хэш ключа.
    const size_t hash = _hash_multimap->hash_key(_key);

    // Приведенный хэш.
    const size_t presented_hash = hash % _hash_multimap->slots_count;

    if (_hash_multimap->slots[presented_hash] != NULL)
    {
        const c_hash_multimap_pack *select_pack = _hash_multimap->slots[presented_hash];
        while (select_pack != NULL)
        {
            if (hash == select_pack->hash)
            {
                if (_key == select_pack->key)
                {
                    if (c_hash_multiset_check(select_pack->data, _data) > 0)
                    {
                        return 1;
                    }
                }
            }
            select_pack = select_pack->next_pack;
        }
    }

    return 0;
}

// Проверка наличия в хэш-мультимножестве заданных данных.
// В случае наличия данных, возвращает > 0.
// В случае, если данных нет, возвращает 0.
// В случае ошибки возвращает < 0.
ptrdiff_t c_hash_multimap_check_data(const c_hash_multimap *const _hash_multimap,
                                     const void *const _data)
{
    if (_hash_multimap == NULL) return -1;
    if (_data == NULL) return -2;

    if (_hash_multimap->datas_count == 0) return 0;

    size_t count = _hash_multimap->keys_count;
    for (size_t s = 0; (s < _hash_multimap->slots_count)&&(count > 0); ++s)
    {
        if (_hash_multimap->slots[s] != NULL)
        {
            const c_hash_multimap_pack *select_pack = _hash_multimap->slots[s];
            while (select_pack != NULL)
            {
                if (c_hash_multiset_check(select_pack->data, _data) > 0)
                {
                    return 1;
                }
                select_pack = select_pack->next_pack;
                --count;
            }
        }
    }

    return 0;
}

// Проходит по всем элементам хэш-мультиотображения и выполняет над ними заданные действия.
// В случае успешного выполнения возвращает > 0.
// Если в хэш-мультиотображении нет элементов, возвращает 0.
// В случае ошибки возвращает < 0.
ptrdiff_t c_hash_multimap_for_each(c_hash_multimap *const _hash_multimap,
                                   void (*const _action_key)(const void *const _key),
                                   void (*const _action_data)(const void *const _data))
{
    if (_hash_multimap == NULL) return -1;
    if (_action_key == NULL) return -2;
    if (_action_data == NULL) return -3;

    if (_hash_multimap->keys_count == 0) return 0;

    size_t count = _hash_multimap->keys_count;
    for (size_t s = 0; (s < _hash_multimap->slots_count)&&(count > 0); ++s)
    {
        if (_hash_multimap->slots[s] != NULL)
        {
            const c_hash_multimap_pack *select_pack = _hash_multimap->slots[s];
            while (select_pack != NULL)
            {
                _action_key( select_pack->key );
                c_hash_multiset_for_each(select_pack->data, _action_data);

                select_pack = select_pack->next_pack;
                --count;
            }
        }
    }

    return 1;
}

// Возвращает количество заданных данных, хранящихся в хэш-мультиотображении по заданному ключу.
// В случае ошибки возвращает 0.
size_t c_hash_multimap_count(const c_hash_multimap *const _hash_multimap,
                             const void *const _key,
                             const void *const _data)
{
    if (_hash_multimap == NULL) return 0;
    if (_key == NULL) return 0;
    if (_data == NULL) return 0;

    if (_hash_multimap->keys_count == 0) return 0;

    // Неприведенный хэш искомого ключа.
    const size_t hash = _hash_multimap->hash_key(_key);

    // Приведенный хэш искомого ключа.
    const size_t presented_hash = hash % _hash_multimap->slots_count;

    if (_hash_multimap->slots[presented_hash] != NULL)
    {
        const c_hash_multimap_pack *select_pack = _hash_multimap->slots[presented_hash];
        while (select_pack != NULL)
        {
            if (hash == select_pack->hash)
            {
                if (_hash_multimap->comp_key(_key, select_pack->key) > 0)
                {
                    return c_hash_multiset_count(select_pack->data, _data);
                }
            }
            select_pack = select_pack->next_pack;
        }
    }

    return 0;
}

// Возвращает количество данных, связанных с заданным ключом.
// В случае ошибки возвращает 0.
size_t c_hash_multimap_count_key(const c_hash_multimap *const _hash_multimap,
                                 const void *const _key)
{
    if (_hash_multimap == NULL) return 0;
    if (_key == NULL) return 0;

    if (_hash_multimap->keys_count == 0) return 0;

    // Неприведенный хэш искомого ключа.
    const size_t hash = _hash_multimap->hash_key(_key);

    // Приведенный хэш искомого ключа.
    const size_t presented_hash = hash % _hash_multimap->slots_count;

    if (_hash_multimap->slots[presented_hash] != NULL)
    {
        const c_hash_multimap_pack *select_pack = _hash_multimap->slots[presented_hash];
        while (select_pack != NULL)
        {
            if (hash == select_pack->hash)
            {
                if (_hash_multimap->comp_key(_key, select_pack->key) > 0)
                {
                    return c_hash_multiset_nodes_count(select_pack->data);
                }
            }
            select_pack = select_pack->next_pack;
        }
    }

    return 0;
}

// Возвращает количество заданных данных в хэш-мультиотображении.
// В случае ошибки возвращает 0.
size_t c_hash_multimap_count_data(const c_hash_multimap *const _hash_multimap,
                                  const void *const _data)
{
    if (_hash_multimap == NULL) return 0;
    if (_data == NULL) return 0;

    if (_hash_multimap->datas_count == 0) return 0;

    size_t data_count = 0;
    size_t count = _hash_multimap->keys_count;
    for (size_t s = 0; (s < _hash_multimap->slots_count)&&(count > 0); ++s)
    {
        if (_hash_multimap->slots[s] != NULL)
        {
            const c_hash_multimap_pack *select_pack = _hash_multimap->slots[s];
            while (select_pack != NULL)
            {
                data_count += c_hash_multiset_count(select_pack->data, _data);

                select_pack = select_pack->next_pack;
                --count;
            }
        }
    }

    return data_count;
}

// Возвращает указатель на массив с набором указателей на данные, которые связаны с заданным ключом.
// Индикатором окончания массива служит (void*) == NULL.
// В случае ошибки возвращает NULL.
// ВАЖНО:
// - массив указателей после использования следует удалять вручную, вызвав free().
// - указатели массива действительны до тех пор, пока хэш-мультиотображение не изменяется.
void **c_hash_multimap_at(c_hash_multimap *const _hash_multimap,
                          const void *const _key)
{
    if (_hash_multimap == NULL) return NULL;
    if (_key == NULL) return NULL;

    if (_hash_multimap->keys_count == 0) return NULL;

    // Неприведенный хэш ключа.
    const size_t hash = _hash_multimap->hash_key(_key);

    // Приведенный хэш ключа.
    const size_t presented_hash = hash % _hash_multimap->slots_count;

    if (_hash_multimap->slots[presented_hash] != NULL)
    {
        const c_hash_multimap_pack *select_pack = _hash_multimap->slots[presented_hash];
        while (select_pack != NULL)
        {
            if (hash == select_pack->hash)
            {
                if (_hash_multimap->comp_key(_key, select_pack->key) > 0)
                {
                    const size_t datas_count = c_hash_multiset_nodes_count(select_pack->data) + 1;
                    if (datas_count == 0)
                    {
                        return NULL;
                    }

                    const size_t datas_size = sizeof(void*) * datas_count;
                    if ( (datas_size == 0) ||
                         (datas_size / sizeof(void*) != datas_count) )
                    {
                        return NULL;
                    }

                    // Попытаемся выделить память под данные.
                    void **const datas = (void**)malloc(datas_size);
                    if (datas == NULL)
                    {
                        return NULL;
                    }

                    // Маркер окончания массива.
                    datas[datas_count - 1] = NULL;

// Делать ли c_hash_multimap работающим с использованием c_hash_multiset, или нет?
// И если да, то как достучаться до сокрытых потрахов c_hash_multiset.
// Пары ключ-значение вставляются в хэш-мультимножество как есть, ключи ДОЛЖНЫ "честно" дублироваться
// Хэш-множество может работать, как хэш-отображение, если генерировать хэш не по всем данным, а по части.

                    // Обойдем все данные пака и занесем указатели на данные в массив datas.
                    size_t n = 0;
                    size_t count = c_hash_multiset_nodes_count(select_pack->data);
                    for (size_t s = 0; (s < c_hash_multiset_slots_count(select_pack->data))&&(count > 0); ++s)
                    {
                        const c_hash_multiset_chain *select_chain = select_pack->data->slots[s];
                        // Перебор всех цепочек.
                        while (select_chain != NULL)
                        {
                            // Перебор всех узлов цепочки.
                            const c_hash_multiset_node *select_node = select_chain->head;
                            while (select_node != NULL)
                            {
                                datas[n++] = select_node->data;
                                select_node = select_node->next_node;
                                --count;
                            }
                            select_chain = select_chain->next_chain;
                        }
                    }
                    return datas;
                }
            }
            select_pack = select_pack->next_pack;
        }
    }

    return NULL;
}

// Возвращает количество слотов в хэш-мультиотображении.
// В случае ошибки возвращает 0.
size_t c_hash_multimap_slots_count(const c_hash_multimap *const _hash_multimap)
{
    if (_hash_multimap == NULL)
    {
        return 0;
    }

    return _hash_multimap->slots_count;
}

// Возвращает количество уникальных ключей в хэш-мультиотображении.
// В случае ошибки возвращает 0.
size_t c_hash_multimap_keys_count(const c_hash_muiltimap *const _hash_multimap)
{
    if (_hash_multimap == NULL)
    {
        return 0;
    }

    return _hash_multimap->keys_count;
}

// Возвращает количество уникальных данных в хэш-мультиотображении.
// В случае ошибки возвращает 0.
size_t c_hash_multimap_datas_count(const c_hash_multimap *const _hash_multimap)
{
    if (_hash_multimap == NULL)
    {
        return 0;
    }

    return _hash_multimap->datas_count;
}

// Возвращает коэф. максимальной загрузки хэш-мультиотображения.
// В случае ошибки возвращает 0.0f.
float c_hash_multimap_max_load_factor(const c_hash_multimap *const _hash_multimap)
{
    if (_hash_multimap == NULL)
    {
        return 0.0f;
    }

    return _hash_multimap->max_load_factor;
}
*/
