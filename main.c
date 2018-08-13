#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "c_hash_multimap.h"

// Проверка возвращаемых значений не выполняется для упрощения.

// Функция генерации хэша по ключу-строке.
size_t hash_key_s(const void *const _key)
{
    if (_key == NULL) return 0;

    const char *c = (char*)_key;
    size_t hash = 0;
    while (*c != 0)
    {
        hash += *(c++);
    }
    return hash;
}

// Функция генерации хэша по данным-float.
size_t hash_data_f(const void *const _data)
{
    if (_data == NULL) return 0;
    const float *const data = (float*)_data;
    return (size_t)( (*data) * 100 );
}

// Функция детального сравнения ключей-строк.
size_t comp_key_s(const void *const _key_a,
                  const void *const _key_b)
{
    if ( (_key_a == NULL) || (_key_b == NULL) ) return 0;

    const char *const key_a = (char*)_key_a;
    const char *const key_b = (char*)_key_b;

    if (strcmp(key_a, key_b) == 0)
    {
        return 1;
    }

    return 0;
}

// Функция детального сравнения данных-float.
size_t comp_data_f(const void *const _data_a,
                   const void *const _data_b)
{
    if ( (_data_a == NULL) || (_data_b == NULL) ) return 0;

    const float *const data_a = (float*)_data_a;
    const float *const data_b = (float*)_data_b;

    if (*data_a == *data_b)
    {
        return 1;
    }

    return 0;
}

// Функция удаления данных-float.
void del_data_f(void *const _data)
{
    if (_data == NULL) return;

    free(_data);

    return;
}

// Функция печати ключа-строки.
void print_key_s(const void *const _key)
{
    if (_key == NULL) return;
    const char *const key = (char*)_key;
    printf("\nkey[%s]: ", key);
    return;
}

// Функция печати данных-float.
void print_data_f(const void *const _data)
{
    if (_data == NULL) return;
    const float *const data = (float*)_data;
    printf("%f ", *data);
    return;
}

int main(int argc, char **argv)
{
    c_hash_multimap *hash_multimap;

    while(1)
    {
        // Создание хэш-мультиотображения.
        hash_multimap = c_hash_multimap_create(hash_key_s,
                                               hash_data_f,
                                               comp_key_s,
                                               comp_data_f,
                                               3,
                                               0.5f);

        // Вставка по трем ключам (статичным) десяти (динамичных) данных.
        /*const char *keys[] = {"One", "Two", "Three"};

        for (size_t i = 0; i < 10; ++i)
        {
            // Выбираем случайный ключ.
            const int key_index = rand() % 3;

            // Делаем и инициализируем данные.
            float *data = (float*)malloc(sizeof(float));
            *data = i;

            // Вставляем.
            c_hash_multimap_insert(hash_multimap, keys[key_index], data);
        }

        // Покажем содержимое.
        c_hash_multimap_for_each(hash_multimap, print_key_s, print_data_f);

        // Удалим все элементы с заданным ключем.
        c_hash_multimap_erase_all(hash_multimap, "Two", NULL, del_data_f);
        */

        // Удаление хэш-мультиотображения.
        c_hash_multimap_delete(hash_multimap, NULL, del_data_f);
    }

    getchar();
    return 0;
}
