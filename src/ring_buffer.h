#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdbool.h>
#include <stdint.h>

#ifndef RING_BUFF_SIZE
#define RING_BUFF_SIZE 1024 // Must be power of two.
#endif
#define RING_BUFF_SIZE_HALF (RING_BUFF_SIZE / 2)
#define RING_BUFF_SIZE_MASK (RING_BUFF_SIZE - 1)

typedef uint16_t RingBufferSize;

typedef struct RingBuffer RingBuffer;
struct RingBuffer {
  uint8_t *const buff;
  volatile RingBufferSize head, tail, count;
};

void rb_Push(RingBuffer *rb, uint8_t data);
uint8_t rb_Pop(RingBuffer *rb);
void rb_Flush(RingBuffer *rb);

inline bool rb_Empty(RingBuffer *rb) { return rb->count == 0; }

inline bool rb_Full(RingBuffer *rb) { return rb->count >= RING_BUFF_SIZE; }

inline bool rb_HalfFull(RingBuffer *rb) {
  return rb->count >= RING_BUFF_SIZE_HALF;
}

#endif /* RING_BUFFER_H */
