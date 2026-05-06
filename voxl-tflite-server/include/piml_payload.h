#ifndef PIML_PAYLOAD_H
#define PIML_PAYLOAD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define PIML_INPUT_MAGIC_NUMBER (0x564F584C)
#define INPUT_BUFFER_SIZE 16

typedef struct piml_input_t {
    uint32_t magic_number;
    int64_t timestamp_ns;
    float input_values[INPUT_BUFFER_SIZE];
} __attribute__((packed)) piml_input_t;

#ifdef __cplusplus
}
#endif

#endif // PIML_PAYLOAD_H