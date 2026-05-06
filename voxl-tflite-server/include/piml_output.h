#ifndef PIML_OUTPUT_H
#define PIML_OUTPUT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define PIML_OUTPUT_MAGIC_NUMBER (0x564F584C)
#define OUTPUT_BUFFER_SIZE 12
typedef struct piml_output_t {
    uint32_t magic_number;
    int64_t timestamp_ns;
    float output_values[OUTPUT_BUFFER_SIZE];
    float preprocess_time_ms;
    float inference_time_ms;
    float postprocess_time_ms;
} __attribute__((packed)) piml_output_t;

#ifdef __cplusplus
}
#endif

#endif // PIML_OUTPUT_H