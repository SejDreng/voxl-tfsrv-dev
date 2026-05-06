#ifndef MPA_ION_BUF_T_H
#define MPA_ION_BUF_T_H

#ifndef CTYPESGEN

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef PLATFORM_QCS6490
#include <errno.h>  // For native handle
#include <stdlib.h> // For native handle
// #include <native_handle.h>
#include <hardware/camera_hardware.h>

// TODO: This is a temporary workaround until we're able to correctly fix the native_handle
// header issue. For now just include the definitions that we need locally
#ifdef __cplusplus
#include <native_handle.h>
#else
typedef struct native_handle
{
    int version;        /* sizeof(native_handle_t) */
    int numFds;         /* number of file-descriptors at &data[0] */
    int numInts;        /* number of ints at &data[numFds] */
    int data[0];        /* numFds + numInts ints */
} native_handle_t;

typedef const native_handle_t* buffer_handle_t;
#endif

#else
#include "hardware/camera3.h"
#endif

////////////////////////////////////////////////////////////////////////////////
// ION BUF
////////////////////////////////////////////////////////////////////////////////

#include "magic_number.h"
#ifndef MPA_ION_BUFFER_MAGIC_NUMBER
#error "MPA_ION_BUFFER_MAGIC_NUMBER not defined!"
#endif

#define BUFFER_SOURCE_UNKNOWN 0
#define BUFFER_SOURCE_LOCAL 1
#define BUFFER_SOURCE_IMPORTED 2

typedef struct {
    int32_t  client_id;   // server’s index of the client (0..N_CLIENT-1)
    int32_t  buffer_id;   // pool index
    uint32_t generation;  // for stale check
} ion_buf_release_msg_t;


#ifdef EN_32BIT_ION_BUF
    // Helper macros for token pasting
    #define CONCAT_INNER(a, b) a##b
    #define CONCAT(a, b) CONCAT_INNER(a, b)
    //padding macro will create unique variables of format: uint8_t paddingXXX[n];
    #define PADDING CONCAT(padding, __LINE__)
    #define PAD_ARM32(n) uint8_t PADDING[n]
#else
    #define PAD_ARM32(n)
#endif

// For 32-bit support, we need to pad mpa_ion_buf_t struct because pointers are 32-bit instead of 64-bit
typedef struct mpa_ion_buf_t
{
    uint32_t magic_number; // Used only when transporting through MPA
    int32_t buffer_id;     // ID of this buffer in the group it was allocated in
    void *vaddress;
    PAD_ARM32(4);
    void *uvHead;
    PAD_ARM32(4);
    void *meta_address;
    PAD_ARM32(4);
    int32_t fd;             // ion memory file descriptor for fast lookup (reserved for future use)
    uint64_t consumerFlags; // flags used by GBM to allocate this
    camera_image_metadata_t img_meta;
    uint64_t size;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t slice;
    uint32_t format;
    struct gbm_bo *bo;
    PAD_ARM32(4);
    buffer_handle_t handle;
    PAD_ARM32(4);
    uint32_t source;
    uint32_t generation;
    uint32_t reserved1;
    uint32_t reserved2;

#ifdef __cplusplus
// Constructor for C++ compatibility
    mpa_ion_buf_t()
    : magic_number(0),
      buffer_id(-1),
      vaddress(nullptr),
      uvHead(nullptr),
      meta_address(nullptr),
      fd(-1),
      consumerFlags(0),
      size(0),
      width(0),
      height(0),
      stride(0),
      slice(0),
      format(0),
      source(0),
      generation(0),
      reserved1(0),
      reserved2(0){}
#endif

} __attribute__((packed)) mpa_ion_buf_t;

#ifdef __cplusplus
}
#endif

#endif // CTYPESGEN

#endif // MPA_ION_BUF_T_H
