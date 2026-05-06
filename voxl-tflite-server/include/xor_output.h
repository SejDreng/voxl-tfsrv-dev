#ifndef XOR_OUTPUT_H
#define XOR_OUTPUT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define XOR_OUTPUT_MAGIC_NUMBER (0x564F584C)

typedef struct xor_output_t {
    uint32_t magic_number;
    int64_t timestamp_ns;
    int output_value;
    float preprocess_time_ms;
    float inference_time_ms;
    float postprocess_time_ms;
} __attribute__((packed)) xor_output_t;

#ifdef __cplusplus
}
#endif

#endif // XOR_OUTPUT_H