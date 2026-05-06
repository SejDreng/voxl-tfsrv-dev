#ifndef VQF_DATA_T_H
#define VQF_DATA_T_H


#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <float.h>


/**
 * Unique 32-bit number used to signal the beginning of a data packet while
 * parsing a data stream.
 */
#include "magic_number.h"
#ifndef VQF_MAGIC_NUMBER
#error "VQF_MAGIC_NUMBER not defined!"
#endif

/**
 * @brief VQF attitude data structure (must match vqf_data_t in FlowServer.hpp)
 */

typedef struct vqf_data_t
{
    uint32_t magic_number; /// Set to VQF_MAGIC_NUMBER for frame syncing
    int64_t timestamp_ns;  /// Timestamp (nanoseconds, monotonic)
    float quat_6d[4];      /// 6D orientation quaternion [w, x, y, z]
    float gyro_bias[3];    /// Estimated gyroscope bias (rad/s)
    float bias_sigma;      /// Bias estimation uncertainty (rad/s)
    uint8_t rest_detected; /// 1 if VQF detected sensor at rest
    uint8_t reserved[3];   /// Padding for alignment
} __attribute__((packed)) vqf_data_t;

/**
 * You don't have to use this read buffer size, but it is HIGHLY
 * recommended to use a multiple of the packet size so that you never read a
 * partial packet which would throw the reader out of sync. Here we use a nice
 * number of 200 packets which is perhaps more than necessary but only takes a
 * little over 8K of memory which is minimal.
 *
 * Note this is NOT the size of the pipe which can hold much more. This is just
 * the read buffer size allocated on the heap into which data from the pipe is
 * read.
 */
#define VQF_RECOMMENDED_READ_BUF_SIZE (sizeof(vqf_data_t) * 200)


/**
 * We recommend VQF servers use a 128k pipe size. This means every client would
 * get their own buffer of over 3.2 seconds of VQF data at 1000hz. Clients can
 * increase this buffer if they wish.
 */
#define VQF_RECOMMENDED_PIPE_SIZE (128 * 1024)

/**
 * @brief      Use this to simultaneously validate that the data from a pipe
 *             contains valid vqf data, find the number of valid packets
 *             contained in a single read from the pipe, and cast the raw data
 *             buffer as an vqf_data_t* for easy access.
 *
 *             This does NOT copy any data and the user does not need to
 *             allocate an vqf_data_t array separate from the pipe read buffer.
 *             The data can be read straight out of the pipe read buffer, much
 *             like reading data directly out of a mavlink_message_t message.
 *
 *             However, this does mean the user should finish processing this
 *             data before returning the pipe data callback which triggers a new
 *             read() from the pipe.
 *
 * @param[in]  data       pointer to pipe read data buffer
 * @param[in]  bytes      number of bytes read into that buffer
 * @param[out] n_packets  number of valid vqf_data_t packets received
 *
 * @return     Returns the same data pointer provided by the first argument, but
 *             cast to an vqf_data_t* struct for convenience. If there was an
 *             error then NULL is returned and n_packets is set to 0
 */
vqf_data_t* pipe_validate_vqf_data_t(char *data, int bytes, int *n_packets);

#ifdef __cplusplus
}
#endif

#endif // VQF_DATA_T_H