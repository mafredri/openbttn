#include <string.h>

#include "ring_buffer.h"

extern inline bool rb_Empty(RingBuffer *rb);
extern inline bool rb_Full(RingBuffer *rb);
extern inline bool rb_HalfFull(RingBuffer *rb);

void rb_Push(RingBuffer *rb, uint8_t data) {
  rb->buff[(rb->head++) & RING_BUFF_SIZE_MASK] = data;

  if (rb_Full(rb)) {
    rb->tail++; // Overflow, we just lost some data.
  } else {
    rb->count++;
  }
}

uint8_t rb_Pop(RingBuffer *rb) {
  uint8_t data;

  if (rb_Empty(rb)) {
    return '\0';
  }

  data = rb->buff[(rb->tail++) & RING_BUFF_SIZE_MASK];
  rb->count--;

  return data;
}

void rb_Flush(RingBuffer *rb) { memset(rb, 0, RING_BUFF_SIZE); }
