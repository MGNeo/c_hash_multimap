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
           k_chains_count;
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

    // Макросы дублирования кода для исключения проверок из циклов.

    // Открытие циклов.
    #define C_HASH_MULTIMAP_CLEAR_BEGIN\
    /* Обойдем слоты */\
    for (size_t s = 0; (s < _hash_multimap->slots_count)&&(count > 0); ++s)\
    {\
        /* Если в слоте есть h-цепочки */\
        if (_hash_multimap->slots[s] != NULL)\
        {\
            /* Обойдем все h-цепочки слота */\
            c_hash_multimap_h_chain *select_h_chain = _hash_multimap->slots[s],\
                                    *delete_h_chain;\
            while (select_h_chain != NULL)\
            {\
                delete_h_chain = select_h_chain;\
                select_h_chain = select_h_chain->next_h_chain;\
                /* Обойдем все k-цепочки */\
                c_hash_multimap_k_chain *select_k_chain = delete_h_chain->head,\
                                        *delete_k_chain;\
                while (select_k_chain != NULL)\
                {\
                    delete_k_chain = select_k_chain;\
                    select_k_chain = select_k_chain->next_k_chain;\
                    /* Обойдем все узлы в k-цепочке */\
                    c_hash_multimap_node *select_node = delete_k_chain->head,\
                                         *delete_node;\
                    while (select_node != NULL)\
                    {\
                        delete_node = select_node;\
                        select_node = select_node->next_node;

    // Закрытие циклов.
    #define C_HASH_MULTIMAP_CLEAR_END\
                        free(delete_node);\
                    }\
                    free(delete_k_chain);\
                }\
                free(delete_h_chain);\
                --count;\
            }\
            _hash_multimap->slots[s] = NULL;\
        }\
    }

    if (_del_key != NULL)
    {
        if (_del_data != NULL)
        {
            C_HASH_MULTIMAP_CLEAR_BEGIN

            _del_key(delete_node->key);
            _del_data(delete_node->data);

            C_HASH_MULTIMAP_CLEAR_END
        } else {
            C_HASH_MULTIMAP_CLEAR_BEGIN

            _del_key(delete_node->key);

            C_HASH_MULTIMAP_CLEAR_END
        }
    } else {
        if (_del_data != NULL)
        {
            C_HASH_MULTIMAP_CLEAR_BEGIN

            _del_data(delete_node->data);

            C_HASH_MULTIMAP_CLEAR_END
        } else {
            C_HASH_MULTIMAP_CLEAR_BEGIN

            C_HASH_MULTIMAP_CLEAR_END
        }
    }

    #undef C_HASH_MULTIMAP_CLEAR_BEGIN
    #undef C_HASH_MULTIMAP_CLEAR_END

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
             (new_slots_size / _slots_count != sizeof(c_hash_multimap_h_chain*)) )
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
                        const size_t presented_k_hash = relocate_h_chain->k_hash % _slots_count;

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

    // Неприведенный хэш ключа.
    const size_t k_hash = _hash_multimap->hash_key(_key);

    // Приведенный хэш ключа.
    const size_t presented_k_hash = k_hash % _hash_multimap->slots_count;

    // Попытаемся найти в требуемом слоте h-цепочку с требуемым неприведенным хэшем ключа.
    c_hash_multimap_h_chain *select_h_chain = _hash_multimap->slots[presented_k_hash];
    while (select_h_chain != NULL)
    {
        if (select_h_chain->k_hash == k_hash)
        {
            break;
        }
        select_h_chain = select_h_chain->next_h_chain;
    }

    // Если найти h-цепочку с требуемым неприведенным хешем ключа не удалось, ее необходимо создать.
    size_t created_h = 0;
    if (select_h_chain == NULL)
    {
        created_h = 1;

        // Пытаемся выделить память под h-цепочку.
        c_hash_multimap_h_chain *const new_h_chain = (c_hash_multimap_h_chain*)malloc(sizeof(c_hash_multimap_h_chain));

        // Если память выделить не удалось.
        if (new_h_chain == NULL)
        {
            return -8;
        }

        // Интегрируем новую h-цепочку в слот.
        new_h_chain->next_h_chain = _hash_multimap->slots[presented_k_hash];
        _hash_multimap->slots[presented_k_hash] = new_h_chain;

        // Заполняем новую h-цепочку.
        new_h_chain->head = NULL;
        new_h_chain->k_chains_count = 0;
        new_h_chain->k_hash = k_hash;

        // Увеличиваем счетчик h-цепочек хэш-мультиотображения.
        ++_hash_multimap->h_chains_count;

        // Выделяем вставленную h-цепочку, это необходимо для единообразной обработки вставки.
        select_h_chain = new_h_chain;
    }

    // Пытаемся найти в выделенной h-цепочке k-цепочку, в которой хранятся узлы с заданным ключомк.
    c_hash_multimap_k_chain *select_k_chain = select_h_chain->head;

    while (select_k_chain != NULL)
    {
        if (_hash_multimap->comp_key(select_k_chain->head->key, _key) > 0)
        {
            break;
        }
        select_k_chain = select_k_chain->next_k_chain;
    }

    // Если не удалось найти в h-цепочке такую k-цепочку, которая хранит узлы с требуемым ключом,
    // формируем новую k-цепочку.
    size_t created_k = 0;
    if (select_k_chain == NULL)
    {
        created_k = 1;

        // Пытаемся выделить память под новую k-цепочку.
        c_hash_multimap_k_chain *const new_k_chain = (c_hash_multimap_k_chain*)malloc(sizeof(c_hash_multimap_k_chain));

        // Если память выделить не удалось, то удаляем h-цепочку, если она была создана, потому что пустая
        // h-цепочка не должна существовать.
        if (new_k_chain == NULL)
        {
            if (created_h == 1)
            {
                // Вырезаем h-цепочку.
                _hash_multimap->slots[presented_k_hash] = select_h_chain->next_h_chain;
                --_hash_multimap->h_chains_count;
                // Освобождаем память.
                free(select_h_chain);
            }
            return -9;
        }
        // Интегрируем новую k-цепочку в выделенную h-цепочку.
        new_k_chain->next_k_chain = select_h_chain->head;
        select_h_chain->head = new_k_chain;

        // Заполняем новую k-цепочку.
        new_k_chain->head = NULL;
        new_k_chain->nodes_count = 0;

        // Увеличиваем счетчик k-цепочек в хэш-мультиотображении.
        ++_hash_multimap->k_chains_count;

        // Выделяем созданную k-цепочку.
        select_k_chain = new_k_chain;
    }

    // Пытаемся выделить память под нвоый узел.
    c_hash_multimap_node *const new_node = (c_hash_multimap_node*)malloc(sizeof(c_hash_multimap_node));

    // Если память под нвоый узел выделить не удалось.
    if (new_node == NULL)
    {
        // Если была создана k-цепочка, удаляем ее, потому что пустая k-цепочка не должна существовать.
        if (created_k == 1)
        {
            // Ампутация.
            select_h_chain->head = select_k_chain->next_k_chain;
            // Уменьшаем счетчик k-цепочек в h-цепочке.
            --select_h_chain->k_chains_count;
            // Уменьшаем счетчик k-цепочек в хэш-мультиотображении.
            --_hash_multimap->k_chains_count;
            // Освобождение памяти.
            free(select_k_chain);
        }
        // Если была создана h-цепочка, удаляем ее, потому что пустая h-цепочка не должна существовать.
        if (created_h == 1)
        {
            // Ампутация.
            _hash_multimap->slots[presented_k_hash] = select_h_chain->next_h_chain;
            // Уменьшаем счетчик h-цепочек в хэш-мультиотображении.
            --_hash_multimap->h_chains_count;
            // Освобождаем память.
            free(select_h_chain);
        }

        return -10;
    }

    // Узел захватывает ключ и данные.
    new_node->key = (void*)_key;
    new_node->data = (void*)_data;
    // Встраиваем узел в выделенную k-цепочку.
    new_node->next_node = select_k_chain->head;
    select_k_chain->head = new_node;
    // Увеличиваем счетчик узлов в выделенной k-цепочке.
    ++select_k_chain->nodes_count;
    // Увеличиваем счетчик узлов в хэш-мультиотображении.
    ++_hash_multimap->nodes_count;

    return 1;
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
    // Если в нужном слоте имеются h-цепочки.
    if (_hash_multimap->slots[presented_k_hash] != NULL)
    {

        // Макросы дублирования кода для исключения проверок из циклов.

        // Открытие цикла.
        #define C_HASH_MULTIMAP_ERASE_ALL_BEGIN\
        /* Обойдем все h-цепочки слота */\
        c_hash_multimap_h_chain *select_h_chain = _hash_multimap->slots[presented_k_hash],\
                                *prev_h_chain = NULL;\
        while (select_h_chain != NULL)\
        {\
            /* Если неприведенный хэш h-цепочки совпал с неприведенным хэшем искомого ключа */\
            if (select_h_chain->k_hash == k_hash)\
            {\
                /* Обойдем k-цепочки в поисках такой, которая хранит узлы с искомым ключом */\
                c_hash_multimap_k_chain *select_k_chain = select_h_chain->head;\
                while (select_k_chain != NULL)\
                {\
                    if (_hash_multimap->comp_key(select_k_chain->head->key, _key) > 0)\
                    {\
                        /* Обойдем и удалим все узлы k-цепочки */\
                        c_hash_multimap_node *select_node = select_k_chain->head,\
                                             *delete_node;\
                        while (select_node != NULL)\
                        {\
                            delete_node = select_node;\
                            select_node = select_node->next_node;

        // Закрытие цикла.
        #define C_HASH_MULTIMAP_ERASE_ALL_END\
                            free(delete_node);\
                        }\
                        /* Ампутируем удаляемую k-цепочку из выделенной h-цепочки */\
                        select_h_chain->head = select_k_chain->next_k_chain;\
                        /* Уменьшим счетчик k-цепочек в h-цепочке */\
                        --select_h_chain->k_chains_count;\
                        /* Уменьшим счетчик k-цепочек в хэш-мультиотображении */\
                        --_hash_multimap->k_chains_count;\
                        /* Уменьшим количество узлов в хэш-мультиотображении на количество узлов в удаляемой k-цепочки */\
                        _hash_multimap->nodes_count -= select_k_chain->nodes_count;\
                        free(select_k_chain);\
                        /* Если h-цепочка опустела, ампутируем ее из слота */\
                        if (select_h_chain->k_chains_count == 0)\
                        {\
                            if (prev_h_chain == NULL)\
                            {\
                                _hash_multimap->slots[presented_k_hash] = select_h_chain->next_h_chain;\
                            } else {\
                                prev_h_chain->next_h_chain = select_h_chain->next_h_chain;\
                            }\
                            --_hash_multimap->h_chains_count;\
                            free(select_h_chain);\
                        }\
                        return 1;\
                    }\
                    select_k_chain = select_k_chain->next_k_chain;\
                }\
            }\
            prev_h_chain = select_h_chain;\
            select_h_chain = select_h_chain->next_h_chain;\
        }

        if (_del_key != NULL)
        {
            if (_del_data != NULL)
            {
                // Заданы функции удаления и для ключа, и для данных.
                C_HASH_MULTIMAP_ERASE_ALL_BEGIN

                _del_key(delete_node->key);
                _del_data(delete_node->data);

                C_HASH_MULTIMAP_ERASE_ALL_END
            } else {
                // Функция удаления задана только для ключа.
                C_HASH_MULTIMAP_ERASE_ALL_BEGIN

                _del_key(delete_node->key);

                C_HASH_MULTIMAP_ERASE_ALL_END
            }
        } else {
            if (_del_data != NULL)
            {
                // Функция удаления задана только для данных.
                C_HASH_MULTIMAP_ERASE_ALL_BEGIN

                _del_data(delete_node->data);

                C_HASH_MULTIMAP_ERASE_ALL_END
            } else {
                // Функции удаления не заданы ни для ключа, ни для данных.
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

    size_t count = _hash_multimap->nodes_count;

    // Макросы дублирования кода для исключения првоерок из циклов.

    // Открытие циклов.
    #define C_HASH_MULTIMAP_FOR_EACH_BEGIN\
    for (size_t s = 0; (s < _hash_multimap->slots_count)&&(count > 0); ++s)\
    {\
        if (_hash_multimap->slots[s] != NULL)\
        {\
            c_hash_multimap_h_chain *select_h_chain = _hash_multimap->slots[s];\
            while (select_h_chain != NULL)\
            {\
                c_hash_multimap_k_chain *select_k_chain = select_h_chain->head;\
                while (select_k_chain != NULL)\
                {\
                    c_hash_multimap_node *select_node = select_k_chain->head;\
                    while (select_node != NULL)\
                    {

    // Закрытие циклов.
    #define C_HASH_MULTIMAP_FOR_EACH_END\
                        select_node = select_node->next_node;\
                        --count;\
                    }\
                    select_k_chain = select_k_chain->next_k_chain;\
                }\
                select_h_chain = select_h_chain->next_h_chain;\
            }\
        }\
    }

    // Заданы действия для ключа и данных.
    if ( (_action_key != NULL) && (_action_data != NULL) )
    {
        C_HASH_MULTIMAP_FOR_EACH_BEGIN

        _action_key(select_node->key);
        _action_data(select_node->data);

        C_HASH_MULTIMAP_FOR_EACH_END
    } else {
        // Задано действие только для ключа.
        if (_action_key != NULL)
        {
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
