#ifndef CIRCULAR_BUFFER_H_
#define CIRCULAR_BUFFER_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// ============================================================================
// 环形 Buffer 结构 - 用于存储序列数据
// ============================================================================

typedef struct
{
    void* data;              // 数据 buffer
    size_t elem_size;        // 每个元素大小
    unsigned int capacity;   // 容量
    unsigned int count;      // 当前元素数量
    unsigned int head;       // 头部索引（写入位置）
    unsigned int tail;       // 尾部索引（读取位置）
    int is_full;             // 是否已满
} circular_buffer_t;

// 初始化环形 buffer
int circular_buffer_init(circular_buffer_t* cb, unsigned int capacity, size_t elem_size);

// 释放环形 buffer
void circular_buffer_destroy(circular_buffer_t* cb);

// 添加元素
int circular_buffer_push(circular_buffer_t* cb, const void* elem);

// 获取指定索引的元素
void* circular_buffer_get(circular_buffer_t* cb, unsigned int index);

// 获取所有数据为数组（用于计算）
void* circular_buffer_get_array(circular_buffer_t* cb, unsigned int* out_count);

// 清空 buffer
void circular_buffer_clear(circular_buffer_t* cb);

// 获取容量
static inline unsigned int circular_buffer_capacity(circular_buffer_t* cb)
{
    return cb->capacity;
}

// 获取当前数量
static inline unsigned int circular_buffer_count(circular_buffer_t* cb)
{
    return cb->count;
}

#endif /* CIRCULAR_BUFFER_H_ */
