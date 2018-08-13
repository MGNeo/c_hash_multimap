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

typedef struct s_c_hash_multimap_k_chain c_hash_multimap_k_chain;

typedef struct s_c_hash_multimap_h_chain c_hash_multimap_h_chain;

struct s_c_hash_multimap_node
{
    c_hash_multimap_node *next_node;

    void *key;
    void *data;

    size_t d_hash;
};

struct s_c_hash_multimap_k_chain
{
    c_hash_multimap_k_chain *next_k_chain;
    c_hash_multimap_node *head;

    size_t nodes_count;
};

struct s_c_hash_multimap_h_chain
{
    c_hash_multimap_h_chain *next_h_chain;
    c_hash_multimap_k_chain *head;

    size_t k_hash,
           k_chain_count;
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
           h_chains_count,
           k_chains_count,
           nodes_count;


    float max_load_factor;

    c_hash_multimap_h_chain **slots;
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

    c_hash_multimap_h_chain **new_slots = NULL;

    if (_slots_count > 0)
    {
        const size_t new_slots_size = _slots_count * sizeof(c_hash_multimap_h_chain*);
        if ( (new_slots_size == 0) ||
             (new_slots_size / _slots_count != sizeof(c_hash_multimap_h_chain*)) )
        {
            return NULL;
        }

        new_slots = (c_hash_multimap_h_chain**)malloc(new_slots_size);
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
    new_hash_multimap->h_chains_count = 0;
    new_hash_multimap->k_chains_count = 0;
    new_hash_multimap->nodes_count = 0;


    new_hash_multimap->max_load_factor = _max_load_factor;

    new_hash_multimap->slots = new_slots;

    return new_hash_multimap;
}

// Удаляет хэш-мультиотображение.
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

// Очищает хэш-мультиотображение ото всех элементов, количество слотов сохраняется.
// В случае успешного очищения возвращает > 0.
// Если очищать не от чего, возвращает 0.
// В случае ошибки возвращает < 0.
ptrdiff_t c_hash_multimap_clear(c_hash_multimap *const _hash_multimap,
                                void (*const _del_key)(void *const _key),
                                void (*const _del_data)(void *const _data))
{
    if (_hash_multimap == NULL)
    {
        return -1;
    }

    // Если очищать не от чего, то ничего не делаем.
    if (_hash_multimap->h_chains_count == 0)
    {
        return 0;
    }

    // Пройдем по всем слотам.
    size_t count = _hash_multimap->h_chains_count;
    for (size_t s = 0; (s < _hash_multimap->slots_count)&&(count > 0); ++s)
    {
        // Если в слоте есть h-цепочки.
        if (_hash_multimap->slots[s] != NULL)
        {
            // Пройдем по всем h-цепочкам.
            c_hash_multimap_h_chain *select_h_chain = _hash_multimap->slots[s],
                                    *delete_h_chain;
            while (select_h_chain != NULL)
            {
                delete_h_chain = select_h_chain;
                select_h_chain = select_h_chain->next_h_chain;

                // Пройдем по всем k-цепочкам.
                c_hash_multimap_k_chain *select_k_chain = delete_h_chain->head,
                                        *delete_k_chain;
                while (select_k_chain != NULL)
                {
                    delete_k_chain = select_k_chain;
                    select_k_chain = select_k_chain->next_k_chain;

                    // Пройдем по всем узлам k-цепочки.
                    c_hash_multimap_node *select_node = delete_k_chain,
                                         *delete_node;
                    while (select_node != NULL)
                    {
                        delete_node = select_node;
                        select_node = select_node->next_node;

                        // Если задана функция удаления ключа.
                        if (_del_key != NULL)
                        {
                            _del_key(delete_node->key);
                        }

                        // Если задана функция удаления данных.
                        if (_del_data != NULL)
                        {
                            _del_data(delete_node->data);
                        }

                        // Удаляем узел.
                        free(delete_node);
                    }

                    // Удаляем k-цепочку.
                    free(delete_k_chain);
                }
                // Удаляем h-цепочку.
                free(delete_h_chain);

                --count;
            }

            // Делаем в слоте отметку о том, что он пустой.
            _hash_multimap->slots[s] = NULL;
        }
    }

    // Обнуляем количество h-цепочек.
    _hash_multimap->h_chains_count = 0;
    // Обнуляем количество k-цепочек.
    _hash_multimap->k_chains_count = 0;
    // Обнуляем количество всех узлов.
    _hash_multimap->nodes_count = 0;

    return 1;
}

// Задает хэш-мультиотображению новое количество слотов.
// Позволяет расширить хэш-мультиотображение с нулем слотов.
// Если в хэш-мультиотображении есть хотя бы один узел (пара ключ-данные), то попытка задать нулевое количество слотов
// считается ошибкой.
// Если хэш-мультиотображение перестраивается функция возвращает > 0.
// Если не перестраивается, функция возвращает 0.
// В случае ошибки возвращает < 0.
ptrdiff_t c_hash_multimap_resize(c_hash_multimap *const _hash_multimap,
                                 const size_t _slots_count)
{
    if (_hash_multimap == NULL)
    {
        return -1;
    }
    if (_slots_count == _hash_multimap->slots_count)
    {
        return 0;
    }

    // Если задано нулевое число слотов.
    if (_slots_count == 0)
    {
        // А в хэш-мультиотображении имеются узлы, это считается ошибкой.
        if (_hash_multimap->nodes_count != 0)
        {
            return -2;
        }

        // Иначе все ок.
        free(_hash_multimap->slots);
        _hash_multimap->slots = NULL;

        _hash_multimap->slots_count = 0;

        return 1;
    } else {
        // Определим количество памяти, необходимое под новые слоты.
        const size_t new_slots_size = _slots_count * sizeof(c_hash_multimap_h_chain*);

        // Контроль переполнения.
        if ( (new_slots_size == 0) ||
             (new_slots_size / _slots_count != sizeof (c_hash_multimap_h_chain)) )
        {
            return -3;
        }

        // Попытаемся выделить память под новые слоты.
        c_hash_multimap_h_chain **const new_slots = (c_hash_multimap_h_chain**)malloc(new_slots_size);

        // Контроль выделения памяти.
        if (new_slots == NULL)
        {
            return -4;
        }

        memset(new_slots, 0, new_slots_size);

        // Если в хэш-мультиотображении есть данные, их необходимо перенести.
        if (_hash_multimap->nodes_count > 0)
        {
            // Проходим по всем слотам.
            size_t count = _hash_multimap->h_chains_count;
            for (size_t s = 0; (s < _hash_multimap->slots_count)&&(count > 0); ++s)
            {
                // Если в слоте есть h-цепочки.
                if (_hash_multimap->slots[s] != NULL)
                {
                    // Проходим по всем h-цепочкам и переносим их в новые слоты.
                    c_hash_multimap_h_chain *select_h_chain = _hash_multimap->slots[s],
                                            *relocate_h_chain;
                    while (select_h_chain != NULL)
                    {
                        relocate_h_chain = select_h_chain;
                        select_h_chain = select_h_chain->next_h_chain;

                        // Вычисляем хэш переносимой h-цепочки, приведенный к новому количеству слотов.
                        const size_t presented_k_hash = relocate_h_chain->k_hash;

                        // Переносим.
                        relocate_h_chain->next_h_chain = new_slots[presented_k_hash];
                        new_slots[presented_k_hash] = relocate_h_chain;

                        --count;
                    }
                }
            }

        }

        // Освобождаем память из-под старых слотов.
        free(_hash_multimap->slots);

        // Используем новые слоты.
        _hash_multimap->slots = new_slots;
        _hash_multimap->slots_count = _slots_count;

        return 2;
    }
}

// Вставляет в хэш-мультиотображение новый элемент (пара ключ-значение).
// В случае успешной вставки возвращает > 0, ключ и данные захватываются хэш-мультиотображением.
// В случае ошибки возвращает < 0, ключ и данные не захватываются хэш-мультиотображением.
ptrdiff_t c_hash_multimap_insert(c_hash_multimap *const _hash_multimap,
                                 const void *const _key,
                                 const void *const _data)
{
    if (_hash_multimap == NULL)
    {
        return -1;
    }
    if (_key == NULL)
    {
        return -2;
    }
    if (_data == NULL)
    {
        return -3;
    }

    // Первым делом контролируем процесс увеличения количества слотов.

    // Если слотов нет вообще.
    if (_hash_multimap->slots_count == 0)
    {
        // Пытаемся расширить слоты.
        if (c_hash_multimap_resize(_hash_multimap, C_HASH_MULTIMAP_0) <= 0)
        {
            return -4;
        }
    } else {
        // Если слоты есть, то при достижении предела загруженности увеличиваем количество слотов.
        const float load_factor = (float)_hash_multimap->h_chains_count / _hash_multimap->slots_count;
        if (load_factor >= _hash_multimap->max_load_factor)
        {
            // Определим новое количество слотов.
            size_t new_slots_count = (size_t)(_hash_multimap->slots_count * 1.75f);
            // Контролируем переполнение.
            if (new_slots_count < _hash_multimap->slots_count)
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
    // Вставляем данные в хэш-мультимножество.

    Тадыщ-бадыщь, свалил домой!
}
