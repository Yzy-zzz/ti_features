#include "utils/circular_buffer.h"

// 初始化环形 buffer
int circular_buffer_init(circular_buffer_t* cb, unsigned int capacity, size_t elem_size)
{
    if (!cb || capacity == 0 || elem_size == 0) {
        return -1;
    }

    cb->data = malloc(capacity * elem_size);
    if (!cb->data) {
        return -1;
    }

    memset(cb->data, 0, capacity * elem_size);
    cb->elem_size = elem_size;
    cb->capacity = capacity;
    cb->count = 0;
    cb->head = 0;
    cb->tail = 0;
    cb->is_full = 0;

    return 0;
}

// 释放环形 buffer
void circular_buffer_destroy(circular_buffer_t* cb)
{
    if (cb && cb->data) {
        free(cb->data);
        cb->data = NULL;
    }
    cb->elem_size = 0;
    cb->capacity = 0;
    cb->count = 0;
    cb->head = 0;
    cb->tail = 0;
    cb->is_full = 0;
}

// 添加元素
int circular_buffer_push(circular_buffer_t* cb, const void* elem)
{
    if (!cb || !elem) {
        return -1;
    }

    unsigned char* ptr = (unsigned char*)cb->data;
    memcpy(ptr + cb->head * cb->elem_size, elem, cb->elem_size);

    cb->head = (cb->head + 1) % cb->capacity;

    if (cb->is_full) {
        // Buffer 已满，移动 tail
        cb->tail = (cb->tail + 1) % cb->capacity;
    } else {
        cb->count++;
        if (cb->count == cb->capacity) {
            cb->is_full = 1;
        }
    }

    return 0;
}

// 获取指定索引的元素
void* circular_buffer_get(circular_buffer_t* cb, unsigned int index)
{
    if (!cb || index >= cb->count) {
        return NULL;
    }

    // 计算实际索引（考虑环形）
    unsigned int actual_index;
    if (cb->is_full) {
        actual_index = (cb->tail + index) % cb->capacity;
    } else {
        actual_index = index;
    }

    unsigned char* ptr = (unsigned char*)cb->data;
    return ptr + actual_index * cb->elem_size;
}

// 获取所有数据为数组（用于计算）
void* circular_buffer_get_array(circular_buffer_t* cb, unsigned int* out_count)
{
    if (!cb || !out_count) {
        return NULL;
    }

    *out_count = cb->count;

    // 分配新数组并复制数据
    void* arr = malloc(cb->count * cb->elem_size);
    if (!arr) {
        return NULL;
    }

    unsigned char* src = (unsigned char*)cb->data;
    unsigned char* dst = (unsigned char*)arr;

    for (unsigned int i = 0; i < cb->count; i++) {
        unsigned int idx = (cb->is_full) ?
            ((cb->tail + i) % cb->capacity) : i;
        memcpy(dst + i * cb->elem_size, src + idx * cb->elem_size, cb->elem_size);
    }

    return arr;
}

// 清空 buffer
void circular_buffer_clear(circular_buffer_t* cb)
{
    if (cb) {
        cb->count = 0;
        cb->head = 0;
        cb->tail = 0;
        cb->is_full = 0;
    }
}
