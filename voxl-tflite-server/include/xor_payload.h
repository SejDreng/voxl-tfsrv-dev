#ifndef XOR_PAYLOAD_H
#define XOR_PAYLOAD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define XOR_INPUT_MAGIC_NUMBER (0x564F584C)
#define INPUT_BUFFER_SIZE 2

typedef struct xor_input_t {
    uint32_t magic_number;
    int64_t timestamp_ns;
    int input_values[INPUT_BUFFER_SIZE];
} __attribute__((packed)) xor_input_t;

#ifdef __cplusplus
}
#endif

#endif // XOR_PAYLOAD_H