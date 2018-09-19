#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "c_hash_multimap.h"

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

int main(int argc, char **argv)
{
    size_t error;
    c_hash_multimap *hash_multimap;

    // Попытаемся создать хэш-мультиотображение.
    hash_multimap = c_hash_multimap_create(hash_key_s,
                                           comp_key_s,
                                           comp_data_f,
                                           10,
                                           0.5f,
                                           &error);
    // Если произошла ошибка, покажем ее.
    if (hash_multimap == NULL)
    {
        printf("create error: %Iu\n", error);
        printf("Program end.\n");
        getchar();
        return -1;
    }

    // Добавим в хэш-мультиотображение пару.
    const char *const key_1 = "One";
    const float data_1 = 1.f;
    {
        const ptrdiff_t r_code = c_hash_multimap_insert(hash_multimap, key_1, &data_1);
        // Покажем результат операции.
        printf("insert[%s, %f]: %Id\n", key_1, data_1, r_code);
    }

    // Добавим в хэш-мультиотображение ту же пару.
    {
        const ptrdiff_t r_code = c_hash_multimap_insert(hash_multimap, key_1, &data_1);
        // Покажем результат операции.
        printf("insert[%s, %f]: %Id\n", key_1, data_1, r_code);
    }

    // Добавим в хэш-мультиотображение другую пару.
    const char *const key_2 = "Two";
    const float data_2 = 2.f;
    {
        const ptrdiff_t r_code = c_hash_multimap_insert(hash_multimap, key_2, &data_2);
        // Покажем результат операции.
        printf("insert[%s, %f]: %Id\n", key_2, data_2, r_code);
    }

    // Используя обход всех элементов, покажем содержимое каждого (каждой пары).
    {
        const ptrdiff_t r_code = c_hash_multimap_for_each(hash_multimap, print_key_s, print_data_f);
        // Если возникла ошибка, покажем ее.
        if (r_code < 0)
        {
            printf("for each error, r_code: %Id\n", r_code);
            printf("Progran end.\n");
            getchar();
            return -2;
        }
    }

    // Удалим все пары с ключом key_1.
    {
        error = 0;
        const size_t d_count = c_hash_multimap_erase_all(hash_multimap, key_1, NULL, NULL, &error);
        // Если возникла ошибка, покажем ее.
        if ( (d_count == 0) && (error > 0) )
        {
            printf("erase all error: %Iu\n", error);
            printf("Program end.\n");
            getchar();
            return -3;
        }
        // Покажем количество удаленных пар.
        printf("erase all[%s]: %Iu\n", key_1, d_count);
    }

    // Используя обход всех элементов, покажем содержимое каждого (каждой пары).
    {
        const ptrdiff_t r_code = c_hash_multimap_for_each(hash_multimap, print_key_s, print_data_f);
        // Если возникла ошибка, покажем ее.
        if (r_code < 0)
        {
            printf("for each error, r_code: %Id\n", r_code);
            printf("Progran end.\n");
            getchar();
            return -4;
        }
    }

    // Удалим хэш-мультиотображение.
    {
        const ptrdiff_t r_code = c_hash_multimap_delete(hash_multimap, NULL, NULL);
        // Если возникла ошибка, покажем ее.
        if (r_code < 0)
        {
            printf("delete error, r_code: %Id\n", r_code);
            printf("Program end.\n");
            getchar();
            return -5;
        }
    }

    getchar();
    return 0;
}


