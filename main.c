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
    printf("[%s]: ", key);
    return;
}

// Функция печати данных-float.
void print_data_f(void *const _data)
{
    if (_data == NULL) return;
    const float *const data = _data;
    printf("%f \n", *data);
    return;
}

// Функция увеличения данных-float на 1.f
void inc_data_f(void *const _data)
{
    if (_data == NULL) return;

    float *const data = _data;
    *data += 1.f;
    return;
}

int main(int argc, char **argv)
{
    c_hash_multimap *hash_multimap;

    while(1)
    {
        // Создание хэш-мультиотображения.
        hash_multimap = c_hash_multimap_create(hash_key_s,
                                               comp_key_s,
                                               comp_data_f,
                                               9,
                                               0.5f);

        // Вставка по трем ключам (статичным) десяти (динамичных) данных.
        const char *keys[] = {"One", "Two", "Three"};

        for (size_t i = 0; i < 10; ++i)
        {
            // Выбираем случайный ключ.
            const int key_index = i % 3;

            // Делаем и инициализируем данные.
            float *data = (float*)malloc(sizeof(float));
            *data = i;

            // Вставляем.
            c_hash_multimap_insert(hash_multimap, keys[key_index], data);
        }

        // Покажем содержимое.
        c_hash_multimap_for_each(hash_multimap, print_key_s, print_data_f);
        printf("\n");

        // Увеличим данные всех пар на 1.f.
        c_hash_multimap_for_each(hash_multimap, NULL, inc_data_f);

        // Покажем содержимое.
        c_hash_multimap_for_each(hash_multimap, print_key_s, print_data_f);
        printf("\n");

        // Удалим все пары с определенным ключом.
        c_hash_multimap_erase_all(hash_multimap, "One", NULL, del_data_f);

        // Покажем содержимое.
        c_hash_multimap_for_each(hash_multimap, print_key_s, print_data_f);
        printf("\n");

        // Удалим одну пару, ключ которой равен "Two", а данные 8.f.
        float h_data = 8.f;
        c_hash_multimap_erase(hash_multimap, "Two", &h_data, NULL, del_data_f);

        // Покажем содержимое.
        c_hash_multimap_for_each(hash_multimap, print_key_s, print_data_f);
        printf("\n");

        // Проверим наличие каждого ключа в хэш-мультиотображении.
        // А так же количество пар с каждым ключом.
        for (size_t k = 0; k < 3; ++k)
        {
            const ptrdiff_t have = c_hash_multimap_key_check(hash_multimap, keys[k]);
            const size_t count = c_hash_multimap_key_count(hash_multimap, keys[k]);
            printf("[%s], have/count: %Id/%Iu\n", keys[k], have, count);
        }
        printf("\n");

        // Проверим наличие и количество определенных пар в хэш-мультиотображении.
        for (size_t k = 0; k < 3; ++k)
        {
            for (float d = 0.f; d < 10.f; d += 1.f)
            {
                const ptrdiff_t have = c_hash_multimap_pair_check(hash_multimap, keys[k], &d);
                const size_t count = c_hash_multimap_pair_count(hash_multimap, keys[k], &d);
                printf("[%s, %f], have/count: %Id/%Iu\n", keys[k], d, have, count);
            }
        }

        // Покажем общую информацию о хэш-мультиотображении.
        printf("slots_count: %Iu\n", c_hash_multimap_slots_count(hash_multimap));
        printf("unique_keys_count: %Iu\n", c_hash_multimap_unique_keys_count(hash_multimap));
        printf("pairs_count: %Iu\n", c_hash_multimap_pairs_count(hash_multimap));
        // Удаление хэш-мультиотображения.
        c_hash_multimap_delete(hash_multimap, NULL, del_data_f);

        printf("\n");
        getchar();
    }

    getchar();
    return 0;
}

