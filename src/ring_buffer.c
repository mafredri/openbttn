#include <string.h>

#include "ring_buffer.h"

extern inline bool ring_buffer_empty(ring_buffer_t *rb);
extern inline bool ring_buffer_full(ring_buffer_t *rb);
extern inline bool ring_buffer_half_full(ring_buffer_t *rb);

void ring_buffer_push(ring_buffer_t *rb, uint8_t data) {
  rb->buff[(rb->head++) & RING_BUFF_SIZE_MASK] = data;

  if (ring_buffer_full(rb)) {
    rb->tail++;  // Overflow, we just lost some data.
  } else {
    rb->count++;
  }
}

uint8_t ring_buffer_pop(ring_buffer_t *rb) {
  uint8_t data;

  if (ring_buffer_empty(rb)) {
    return '\0';
  }

  data = rb->buff[(rb->tail++) & RING_BUFF_SIZE_MASK];
  rb->count--;

  return data;
}

void ring_buffer_flush(ring_buffer_t *rb) { memset(rb, 0, RING_BUFF_SIZE); }
