#include <string.h>

#include "ring_buffer.h"

extern inline bool rb_Empty(RingBuffer *rb);
extern inline bool rb_HalfFull(RingBuffer *rb);

bool rb_Push(RingBuffer *rb, uint8_t data) {
  RingBufferSize next = (rb->head + 1) & RING_BUFF_SIZE_MASK;
  if (next != rb->tail) {
    rb->buff[rb->head] = data;
    rb->head = next;
    return true;
  }

  return false;
}

uint8_t rb_Pop(RingBuffer *rb) {
  uint8_t data;

  if (rb_Empty(rb)) {
    return '\0';
  }

  data = rb->buff[rb->tail];
  rb->tail = (rb->tail + 1) & RING_BUFF_SIZE_MASK;

  return data;
}

void rb_Flush(RingBuffer *rb) {
  memset(rb->buff, 0, RING_BUFF_SIZE);
  rb->count = 0;
  rb->tail = rb->head;
}
