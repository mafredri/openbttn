#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdbool.h>
#include <stdint.h>

#define RING_BUFF_SIZE 1024  // Must be power of two.
#define RING_BUFF_SIZE_HALF (RING_BUFF_SIZE / 2)
#define RING_BUFF_SIZE_MASK (RING_BUFF_SIZE - 1)

typedef uint16_t ring_buffer_size_t;

struct ring_buffer_t {
  uint8_t *const buff;
  volatile ring_buffer_size_t head, tail, count;
};

typedef struct ring_buffer_t ring_buffer_t;

void ring_buffer_push(ring_buffer_t *rb, uint8_t data);
uint8_t ring_buffer_pop(ring_buffer_t *rb);
void ring_buffer_flush(ring_buffer_t *rb);

inline bool ring_buffer_empty(ring_buffer_t *rb) { return rb->count == 0; }

inline bool ring_buffer_full(ring_buffer_t *rb) {
  return rb->count >= RING_BUFF_SIZE;
}

inline bool ring_buffer_half_full(ring_buffer_t *rb) {
  return rb->count >= RING_BUFF_SIZE_HALF;
}

#endif /* RING_BUFFER_H */
