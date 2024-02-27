/*
 * circbuffer.h
 *
 *  Created on: 27 Mar 2021
 *      Author: mark
 *   Copyright  2021..2024 Neostim
 */

#ifndef INC_CIRCBUFFER_H_
#define INC_CIRCBUFFER_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void *storage;
    uint32_t size;
    uint32_t wi, ri;
} CircBuffer;


void CircBuffer_init(CircBuffer *, uint8_t *storage, uint32_t size);
void CircBuffer_clear(CircBuffer *);
bool CircBuffer_isEmpty(CircBuffer const *);
int  CircBuffer_print(CircBuffer const *, int (*f)(char const *fmt, ...));
uint32_t CircBuffer_availableSpace(CircBuffer const *);
uint32_t CircBuffer_availableData(CircBuffer const *);
uint32_t CircBuffer_write(CircBuffer *, uint8_t const *src, uint32_t nr_of_bytes);
uint32_t CircBuffer_peek(CircBuffer const *, uint8_t *dst, uint32_t nr_of_bytes);
uint32_t CircBuffer_read(CircBuffer *, uint8_t *dst, uint32_t nr_of_bytes);

#ifdef __cplusplus
}
#endif

#endif
