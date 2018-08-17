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
};

// Цепочка содержит узлы с одинаковым ключом.
struct s_c_hash_multimap_chain
{
    c_hash_multimap_chain *next_chain;
    c_hash_multimap_node *head;

    size_t k_hash,
           nodes_count;
};

struct s_c_hash_multimap
{
    // Функция генерации хэша по ключу.
    size_t (*hash_key)(const void *const _key);

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
                                        size_t (*const _comp_key)(const void *const _key_a,
                                                                  const void *const _key_b),
                                        size_t (*const _comp_data)(const void *const _data_a,
                                                                   const void *const _data_b),
                                        const size_t _slots_count,
                                        const float _max_load_factor)
{
    if (_hash_key == NULL) return NULL;

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
    new_hash_multimap->comp_key = _comp_key;
    new_hash_multimap->comp_data = _comp_data;

    new_hash_multimap->slots_count = _slots_count;
    new_hash_multimap->chains_count = 0;
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
    if (_hash_multimap->chains_count == 0)
    {
        return 0;
    }

    size_t count = _hash_multimap->chains_count;

    // Макросы дублирования кода для избавления от проверок в циклах.

    // Открытие циклов.
    #define C_HASH_MULTIMAP_CLEAR_BEGIN\
    for (size_t s = 0; (s < _hash_multimap->slots_count)&&(count > 0); ++s)\
    {\
        if (_hash_multimap->slots[s] != NULL)\
        {\
            c_hash_multimap_chain *select_chain = _hash_multimap->slots[s],\
                                  *delete_chain;\
            /* Обойдем все цепочки слота */\
            while (select_chain != NULL)\
            {\
                delete_chain = select_chain;\
                select_chain = select_chain->next_chain;\
                /* Обойдем все узлы цепочки */\
                c_hash_multimap_node *select_node = delete_chain->head,\
                                     *delete_node;\
                while (select_node != NULL)\
                {\
                    delete_node = select_node;\
                    select_node = select_node->next_node;\

    // Закрытие циклов.
    #define C_HASH_MULTIMAP_CLEAR_END\
                    /* Освободим память из-под узла */\
                    free(delete_node);\
                }\
                /* Освободим память из-под цепочки */\
                free(delete_chain);\
                --count;\
            }\
            _hash_multimap->slots[s] = NULL;\
        }\
    }

    if (_del_key != NULL)
    {
        if (_del_data != NULL)
        {
            // Функция удаления задана и для ключа, и для данных.
            C_HASH_MULTIMAP_CLEAR_BEGIN

            _del_key(delete_node->key);
            _del_data(delete_node->data);

            C_HASH_MULTIMAP_CLEAR_END
        } else {
            // Функция удаления задана только для ключа.
            C_HASH_MULTIMAP_CLEAR_BEGIN

            _del_key(delete_node->key);

            C_HASH_MULTIMAP_CLEAR_END
        }
    } else {
        if (_del_data != NULL)
        {
            // Функция удаления задана только для данных.
            C_HASH_MULTIMAP_CLEAR_BEGIN

            _del_data(delete_node->data);

            C_HASH_MULTIMAP_CLEAR_END
        } else {
            // Функция удаления не задана ни для ключа, ни для данных.
            C_HASH_MULTIMAP_CLEAR_BEGIN

            C_HASH_MULTIMAP_CLEAR_END
        }
    }

    #undef C_HASH_MULTIMAP_CLEAR_BEGIN
    #undef C_HASH_MULTIMAP_CLEAR_END

    _hash_multimap->chains_count = 0;
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
        // А в хэш-мультиотображении имеются узлы.
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
        // Если задано ненулевое число слотов.

        // Определим количество памяти, необходимое под новые слоты.
        const size_t new_slots_size = _slots_count * sizeof(c_hash_multimap_chain*);

        // Контроль переполнения.
        if ( (new_slots_size == 0) ||
             (new_slots_size / _slots_count != sizeof(c_hash_multimap_chain*)) )
        {
            return -3;
        }

        // Попытаемся выделить память под новые слоты.
        c_hash_multimap_chain **const new_slots = (c_hash_multimap_chain**)malloc(new_slots_size);

        // Контроль успешности выделения памяти.
        if (new_slots == NULL)
        {
            return -4;
        }

        memset(new_slots, 0, new_slots_size);

        // Если в хэш-мультиотображении есть данные, их необходимо перенести.
        if (_hash_multimap->nodes_count > 0)
        {
            // Проходим по всем слотам.
            size_t count = _hash_multimap->chains_count;
            for (size_t s = 0; (s < _hash_multimap->slots_count)&&(count > 0); ++s)
            {
                if (_hash_multimap->slots[s] != NULL)
                {
                    // Проходим по всем цепочкам слота.
                    c_hash_multimap_chain *select_chain = _hash_multimap->slots[s],
                                          *relocate_chain;
                    while (select_chain != NULL)
                    {
                        relocate_chain = select_chain;
                        select_chain = select_chain->next_chain;

                        // Вычисляем хэш переносимой цепочки, приведенный к новому количеству слотов.
                        const size_t presented_k_hash = relocate_chain->k_hash % _slots_count;

                        // Переносим.
                        relocate_chain->next_chain = new_slots[presented_k_hash];
                        new_slots[presented_k_hash] = relocate_chain;

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
        const float load_factor = (float)_hash_multimap->chains_count / _hash_multimap->slots_count;
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

    // Неприведенный хэш ключа.
    const size_t k_hash = _hash_multimap->hash_key(_key);

    // Приведенный хэш ключа.
    const size_t presented_k_hash = k_hash % _hash_multimap->slots_count;

    // Попытаемся найти с нужном слоте цепочку, которая хранит узлы с аналогичным ключом.
    c_hash_multimap_chain *select_chain = _hash_multimap->slots[presented_k_hash];
    while (select_chain != NULL)
    {
        if (select_chain->k_hash == k_hash)
        {
            if (_hash_multimap->comp_key(select_chain->head->key, _key) > 0)
            {
                break;
            }
        }
        select_chain = select_chain->next_chain;
    }

    // Если цепочки, которая хранит узлы с заданным ключом, нет, то создаем ее.
    size_t created = 0;
    if (select_chain == NULL)
    {
        created = 1;

        // Пытаемся выделить память под цепочку.
        c_hash_multimap_chain *const new_chain = (c_hash_multimap_chain*)malloc(sizeof(c_hash_multimap_chain));

        // Если память выделить не удалось.
        if (new_chain == NULL)
        {
            return -8;
        }

        // Интегрируем новую цепочку в слот и выделяем ее, это необходимо для дальнейшей единообразной обработки.
        new_chain->next_chain = _hash_multimap->slots[presented_k_hash];
        _hash_multimap->slots[presented_k_hash] = new_chain;

        // Заполняем новую цепочку.
        new_chain->head = NULL;
        new_chain->nodes_count = 0;
        new_chain->k_hash = k_hash;

        // Увеличиваем счетчик цепочек в хэш-мультиотображении.
        ++_hash_multimap->chains_count;

        // Выделяем вставленную цепочку.
        select_chain = new_chain;
    }

    // Пытаемся выделить память под новый узел.
    c_hash_multimap_node *const new_node = (c_hash_multimap_node*)malloc(sizeof(c_hash_multimap_node));

    // Если память под узел выделить не удалось.
    if (new_node == NULL)
    {
        // И если цепочка была создана.
        if (created == 1)
        {
            // Ампутация созданной цепочки, потому что цепочка без узлов не должна существовать.
            _hash_multimap->slots[presented_k_hash] = select_chain->next_chain;
            // Уменьшаем счетчик цепочек в хэш-мультиотображении.
            --_hash_multimap->chains_count;
            // Освобождаем память из-под цепочки.
            free(select_chain);
        }

        return -10;
    }

    // Узел захватывает ключ и данные.
    new_node->key = (void*)_key;
    new_node->data = (void*)_data;
    // Встраиваем узел в выделенную цепочку.
    new_node->next_node = select_chain->head;
    select_chain->head = new_node;
    // Увеличиваем счетчик узлов выделенной цепочки.
    ++select_chain->nodes_count;
    // Увеличичваем счетчик узлов в хэш-мультиотоюражении.
    ++_hash_multimap->nodes_count;

    return 1;
}

// Удаляет заданную пару из хэш-мультиотображения.
// В случае успещного удаления возвращает > 0.
// Если такой пары нет, возвращает 0.
// В случае ошибки возвращает < 0.
ptrdiff_t c_hash_multimap_erase(c_hash_multimap *const _hash_multimap,
                                const void *const _key,
                                const void *const _data,
                                void (*const _del_key)(void *const _key),
                                void (*const _del_data)(void *const _data))
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
    if (_hash_multimap->nodes_count == 0)
    {
        return 0;
    }

    // Неприведенный хэш ключа.
    const size_t k_hash = _hash_multimap->hash_key(_key);

    // Приведенный хэш ключа.
    const size_t presented_k_hash = k_hash % _hash_multimap->slots_count;

    if (_hash_multimap->slots[presented_k_hash] != NULL)
    {
        // Перебираем цепочки слота.
        c_hash_multimap_chain *select_chain = _hash_multimap->slots[presented_k_hash],
                              *prev_chain = NULL;
        while (select_chain != NULL)
        {
            if (select_chain->k_hash == k_hash)
            {
                if (_hash_multimap->comp_key(select_chain->head->key, _key) > 0)
                {
                    // Перебираем узлы цепочки в поисках такого узла.
                    c_hash_multimap_node *select_node = select_chain->head,
                                         *prev_node = NULL;
                    while (select_node != NULL)
                    {
                        if (_hash_multimap->comp_data(select_node->data, _data) > 0)
                        {
                            // Нашли требуемый узел.

                            // Ампутируем узел из цепочки.
                            if (prev_node == NULL)
                            {
                                select_chain->head = select_node->next_node;
                            } else {
                                prev_node->next_node = select_node->next_node;
                            }

                            // Уменьшаем счетчик узлов выделенной цепочки.
                            --select_chain->nodes_count;
                            // Уменьшаем счетчик узлов хэш-мультиотображения.
                            --_hash_multimap->nodes_count;

                            // Если задана функция удаления для ключа.
                            if (_del_key != NULL)
                            {
                                _del_key(select_node->key);
                            }

                            // Если задана функция удаления для данных.
                            if (_del_data != NULL)
                            {
                                _del_data(select_node->data);
                            }

                            // Освобождаем из-под узла память.
                            free(select_node);

                            // Если цепочка опустела, удаляем ее.
                            if (select_chain->nodes_count == 0)
                            {
                                // Ампутация из слота.
                                if (prev_chain == NULL)
                                {
                                    _hash_multimap->slots[presented_k_hash] = select_chain->next_chain;
                                } else {
                                    prev_chain->next_chain = select_chain->next_chain;
                                }

                                // Уменьшаем счетчик цепочек хэш-мультиотображения.
                                --_hash_multimap->chains_count;

                                // Освобождаем из-под цепочки память.
                                free(select_chain);
                            }

                            return 1;
                        }
                        prev_node = select_node;
                        select_node = select_node->next_node;
                    }
                }
            }
            prev_chain = select_chain;
            select_chain = select_chain->next_chain;
        }
    }

    return 0;
}

// Удаляет из хэш-мультиотображения все пары с заданным ключом.
// В случае успешного удаления возвращает > 0.
// Если пар с заданным ключом в хэш-мультиотображении не оказалось, возвращает 0.
// В случае ошибки возвращает < 0.
ptrdiff_t c_hash_multimap_erase_all(c_hash_multimap *const _hash_multimap,
                                    const void *const _key,
                                    void (*const _del_key)(void *const _key),
                                    void (*const _del_data)(void *const _data))
{
    if (_hash_multimap == NULL)
    {
        return -1;
    }
    if (_key == NULL)
    {
        return -2;
    }

    if (_hash_multimap->nodes_count == 0)
    {
        return 0;
    }

    // Неприведенный хэш ключа.
    const size_t k_hash = _hash_multimap->hash_key(_key);

    // Приведенный хэш ключа.
    const size_t presented_k_hash = k_hash % _hash_multimap->slots_count;

    // Если в нужном слоте имеются цепочки.
    if (_hash_multimap->slots[presented_k_hash] != NULL)
    {
        // Макросы дублирования кода для исключения проверок из циклов.

        // Открытие циклов.
        #define C_HASH_MULTIMAP_ERASE_ALL_BEGIN\
        /* Перебираем цепочки в поисках такой, которая хранит узлы с аналогичным ключом */\
        c_hash_multimap_chain *select_chain = _hash_multimap->slots[presented_k_hash],\
                              *prev_chain = NULL;\
        while (select_chain != NULL)\
        {\
            if (select_chain->k_hash == k_hash)\
            {\
                if (_hash_multimap->comp_key(select_chain->head->key, _key) > 0)\
                {\
                    /* Обойдем все узлы цепочки и удалим их */\
                    c_hash_multimap_node *select_node = select_chain->head,\
                                         *delete_node;\
                    while (select_node != NULL)\
                    {\
                        delete_node = select_node;\
                        select_node = select_node->next_node;

        // Закрытие циклов.
        #define C_HASH_MULTIMAP_ERASE_ALL_END\
                        free(delete_node);\
                    }\
                    /* Ампутируем цепочку */\
                    if (prev_chain == NULL)\
                    {\
                        _hash_multimap->slots[presented_k_hash] = select_chain->next_chain;\
                    } else {\
                        prev_chain->next_chain = select_chain->next_chain;\
                    }\
                    /* Уменьшаем счетчик цепочек хэш-мультиотображения */\
                    --_hash_multimap->chains_count;\
                    /* Уменьшаем счетчик узлов хэш-мультиотображения на количество узлов удаляемой цепочки */\
                    _hash_multimap->nodes_count -= select_chain->nodes_count;\
                    /* Освобождаем память */\
                    free(select_chain);\
                    return 1;\
                }\
            }\
            prev_chain = select_chain;\
            select_chain = select_chain->next_chain;\
        }

        if (_del_key != NULL)
        {
            if (_del_data != NULL)
            {
                C_HASH_MULTIMAP_ERASE_ALL_BEGIN

                _del_key(delete_node->key);
                _del_data(delete_node->data);

                C_HASH_MULTIMAP_ERASE_ALL_END
            } else {
                C_HASH_MULTIMAP_ERASE_ALL_BEGIN

                _del_key(delete_node->key);

                C_HASH_MULTIMAP_ERASE_ALL_END
            }
        } else {
            if (_del_data != NULL)
            {
                C_HASH_MULTIMAP_ERASE_ALL_BEGIN

                _del_data(delete_node->data);

                C_HASH_MULTIMAP_ERASE_ALL_END
            } else {
                C_HASH_MULTIMAP_ERASE_ALL_BEGIN

                C_HASH_MULTIMAP_ERASE_ALL_END
            }
        }

        #undef C_HASH_MULTIMAP_ERASE_ALL_BEGIN
        #undef C_HASH_MULTIMAP_ERASE_ALL_END
    }

    return 0;
}

// Обход всеъ пар хэш-мультиотображения и выполнение над ключами и данными пар заданных действий.
// Должно быть задано действие хотя бы для ключа, или хотя бы для данных.
// Ключи нельзя удалять и менять.
// Данные нельзя удалять, но можно менять.
// В случае успеха возвращает > 0.
// Если в хэш-мультиотображении нет пар, возвращает 0.
// В случае ошибки возвращает < 0.
ptrdiff_t c_hash_multimap_for_each(c_hash_multimap *const _hash_multimap,
                                   void (*const _action_key)(const void *const _key),
                                   void (*const _action_data)(void *const _data))
{
    if (_hash_multimap == NULL)
    {
        return -1;
    }

    if ( (_action_key == NULL) && (_action_data == NULL) )
    {
        return -2;
    }

    size_t count = _hash_multimap->chains_count;

    // Макросы дублирования кода для избавления от проверок внутри циклов.

    // Открытие циклов.
    #define C_HASH_MULTIMAP_FOR_EACH_BEGIN\
    for (size_t s = 0; (s < _hash_multimap->slots_count)&&(count > 0); ++s)\
    {\
        if (_hash_multimap->slots[s] != NULL)\
        {\
            c_hash_multimap_chain *select_chain = _hash_multimap->slots[s];\
            while (select_chain != NULL)\
            {\
                c_hash_multimap_node *select_node = select_chain->head;\
                while (select_node != NULL)\
                {

    // Закрытие циклов.
    #define C_HASH_MULTIMAP_FOR_EACH_END\
                    select_node = select_node->next_node;\
                }\
                select_chain = select_chain->next_chain;\
                --count;\
            }\
        }\
    }

    if ( (_action_key != NULL) && (_action_data != NULL) )
    {
        // Заданы оба действия.
        C_HASH_MULTIMAP_FOR_EACH_BEGIN

        _action_key(select_node->key);
        _action_data(select_node->data);

        C_HASH_MULTIMAP_FOR_EACH_END
    } else {
        if (_action_key != NULL)
        {
            // Задано действие только для ключей.
            C_HASH_MULTIMAP_FOR_EACH_BEGIN

            _action_key(select_node->key);

            C_HASH_MULTIMAP_FOR_EACH_END
        } else {
            // Задано действие только для данных.
            C_HASH_MULTIMAP_FOR_EACH_BEGIN

            _action_data(select_node->data);

            C_HASH_MULTIMAP_FOR_EACH_END
        }
    }

    #undef C_HASH_MULTIMAP_FOR_EACH_BEGIN
    #undef C_HASH_MULTIMAP_FOR_EACH_END

    return 1;
}

// Проверяет, есть ли такой ключ в хэш-мультиотображении.
// Если есть, возвращает > 0.
// Если нет, возвращает 0.
// В случае ошибки возвращает < 0.
ptrdiff_t c_hash_multimap_key_check(const c_hash_multimap *const _hash_multimap,
                                    const void *const _key)
{
    if (_hash_multimap == NULL)
    {
        return -1;
    }
    if (_key == NULL)
    {
        return -2;
    }

    if (_hash_multimap->nodes_count == 0)
    {
        return 0;
    }

    // Неприведенный хэш ключа.
    const size_t k_hash = _hash_multimap->hash_key(_key);

    // Приведенный хэш ключа.
    const size_t presented_k_hash = k_hash % _hash_multimap->slots_count;

    if (_hash_multimap->slots[presented_k_hash] != NULL)
    {
        const c_hash_multimap_chain *select_chain = _hash_multimap->slots[presented_k_hash];
        while (select_chain != NULL)
        {
            if (select_chain->k_hash == k_hash)
            {
                if (_hash_multimap->comp_key(select_chain->head->key, _key) > 0)
                {
                    return 1;
                }
            }
            select_chain = select_chain->next_chain;
        }
    }

    return 0;
}

// Возвращает количество пар с заданным ключом в хэш-мультиотображении.
// В случае ошибки возвращает 0.
size_t c_hash_multimap_key_count(const c_hash_multimap *const _hash_multimap,
                                 const void *const _key)
{
    if (_hash_multimap == NULL)
    {
        return 0;
    }

    if (_key == NULL)
    {
        return 0;
    }

    if (_hash_multimap->nodes_count == 0)
    {
        return 0;
    }

    // Неприведенный хэш искомого ключа.
    const size_t k_hash = _hash_multimap->hash_key(_key);

    // Приведенный хэш искомого ключа.
    const size_t presented_k_hash = k_hash % _hash_multimap->slots_count;

    if (_hash_multimap->slots[presented_k_hash] != NULL)
    {
        const c_hash_multimap_chain *select_chain = _hash_multimap->slots[presented_k_hash];
        while (select_chain != NULL)
        {
            if (select_chain->k_hash == k_hash)
            {
                if (_hash_multimap->comp_key(select_chain->head->key, _key) > 0)
                {
                    return select_chain->nodes_count;
                }
            }
            select_chain = select_chain->next_chain;
        }
    }

    return 0;
}

// Проверка наличия заданной пары в хэш-мультиотображении.
// В случае наличия пары возвращает > 0.
// В случае отсутствия пары возвращает 0.
// В случае ошибки возвращает < 0.
ptrdiff_t c_hash_multimap_pair_check(const c_hash_multimap *const _hash_multimap,
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

    if (_hash_multimap->nodes_count == 0)
    {
        return 0;
    }

    // Неприведенный хэш ключа.
    const size_t k_hash = _hash_multimap->hash_key(_key);

    // Приведенный хэш ключа.
    const size_t presented_k_hash = k_hash % _hash_multimap->slots_count;

    if (_hash_multimap->slots[presented_k_hash] != NULL)
    {
        const c_hash_multimap_chain *select_chain = _hash_multimap->slots[presented_k_hash];
        while (select_chain != NULL)
        {
            if (select_chain->k_hash == k_hash)
            {
                if (_hash_multimap->comp_key(select_chain->head->key, _key) > 0)
                {
                    const c_hash_multimap_node *select_node = select_chain->head;
                    while (select_node != NULL)
                    {
                        if (_hash_multimap->comp_data(select_node->data, _data) > 0)
                        {
                            return 1;
                        }
                        select_node = select_node->next_node;
                    }
                }
            }
            select_chain = select_chain->next_chain;
        }
    }
    return 0;
}

// Возвращает количество пар с заданным ключем и данными.
// В случае ошибки возвращает 0.
size_t c_hash_multimap_pair_count(const c_hash_multimap *const _hash_multimap,
                                  const void *const _key,
                                  const void *const _data)
{
    if (_hash_multimap == NULL)
    {
        return 0;
    }
    if (_key == NULL)
    {
        return 0;
    }
    if (_data == NULL)
    {
        return 0;
    }

    if (_hash_multimap->nodes_count == 0)
    {
        return 0;
    }

    // Неприведенный хэш ключа.
    const size_t k_hash = _hash_multimap->hash_key(_key);

    // Приведенный хэш ключа.
    const size_t presented_k_hash = k_hash % _hash_multimap->slots_count;

    if (_hash_multimap->slots[presented_k_hash] != NULL)
    {
        const c_hash_multimap_chain *select_chain = _hash_multimap->slots[presented_k_hash];
        while (select_chain != NULL)
        {
            if (select_chain->k_hash == k_hash)
            {
                if (_hash_multimap->comp_key(select_chain->head->key, _key) > 0)
                {
                    size_t count = 0;
                    const c_hash_multimap_node *select_node = select_chain->head;
                    while (select_node != NULL)
                    {
                        if (_hash_multimap->comp_data(select_node->data, _data) > 0)
                        {
                            ++count;
                        }
                        select_node = select_node->next_node;
                    }
                    return count;
                }
            }
            select_chain = select_chain->next_chain;
        }
    }
    return 0;
}

// Возвращает количество слотов хэш-мультиотображения.
// В случае ошибки возвращает 0.
size_t c_hash_multimap_slots_count(const c_hash_multimap *const _hash_multimap)
{
    if (_hash_multimap == NULL)
    {
        return 0;
    }

    return _hash_multimap->slots_count;
}

// Возвращает количество цепочек в хэш-мультимножестве.
// В случае ошибки возвращает 0.
size_t c_hash_multimap_unique_keys_count(const c_hash_multimap *const _hash_multimap)
{
    if (_hash_multimap == NULL)
    {
        return 0;
    }

    return _hash_multimap->chains_count;
}

// Возвращает количество узлов в хэш-мультимножестве.
// В случае ошибки возвращает 0.
size_t c_hash_multimap_pairs_count(const c_hash_multimap *const _hash_multimap)
{
    if (_hash_multimap == NULL)
    {
        return 0;
    }

    return _hash_multimap->nodes_count;
}
