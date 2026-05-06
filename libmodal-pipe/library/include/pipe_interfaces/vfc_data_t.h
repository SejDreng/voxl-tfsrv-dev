#ifndef VFC_DATA_T_H
#define VFC_DATA_T_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

////////////////////////////////////////////////////////////////////////////////
// VFC
////////////////////////////////////////////////////////////////////////////////

/**
 * Unique 32-bit number used to signal the beginning of a data packet while
 * parsing a data stream. If this were to be cast as a float it would have a
 * value of 5.7x10^13 which is an impossible value for translations/rotation
 * readings making it unique as an identifier.
 *
 */
#include "magic_number.h"
#ifndef VFC_MAGIC_NUMBER
#error "VFC_MAGIC_NUMBER not defined!"
#endif
#ifndef EXT_VFC_MAGIC_NUMBER
#error "EXT_VFC_MAGIC_NUMBER not defined!"
#endif

// VFC submodes. This is not an exhaustive list and custom values not
// included here can be used as long as the server and client both agree on the
// submode definition.
#define VFC_SM_NONE              0
#define VFC_SM_THRUST_ATTITUDE   1
#define VFC_SM_ALTITUDE_ATTITUDE 2
#define VFC_SM_ALTITUDE_FLOW     3
#define VFC_SM_POSITION          4
#define VFC_SM_TRAJ              5
#define VFC_SM_BACKTRACK         6
// NOTE: when updating this list, also update pipe_vfc_submode_to_string()
// and pipe_vfc_submode_to_string_osd() in src/interfaces.c to print the new name

    /**
     * This is the data structure that voxl flight controller (VFC) makes available
     * to indicate current status.
     */
    typedef struct vfc_data_t
    {
        uint32_t magic_number; ///< Unique 32-bit number used to signal the beginning of a VFC status packet while parsing a data stream.
        bool altitude_ok;
        bool flow_ok;
        bool position_ok;
        float backtrack_seconds;
        bool backtrack_active;
        int8_t desired_submode;
        int8_t actual_submode;
    } __attribute__((packed)) vfc_data_t;

/**
 * You don't have to use this read buffer size, but it is HIGHLY recommended to
 * use a multiple of the packet size so that you never read a partial packet
 * which would throw the reader out of sync.
 *
 * Note this is NOT the size of the pipe which can hold much more. This is just
 * the read buffer size allocated on the heap into which data from the pipe is
 * read.
 */
#define VFC_RECOMMENDED_READ_BUF_SIZE (sizeof(vfc_data_t) * 32)

    /**
     * @brief      Use this to simultaneously validate that the bytes from a pipe
     *             contains valid data, find the number of valid packets
     *             contained in a single read from the pipe, and cast the raw data
     *             buffer as a vfc_data_t* for easy access.
     *
     *             This does NOT copy any data and the user does not need to
     *             allocate a vfc_data_t array separate from the pipe read buffer.
     *             The data can be read straight out of the pipe read buffer, much
     *             like reading data directly out of a mavlink_message_t message.
     *
     *             However, this does mean the user should finish processing this
     *             data before returning the pipe data callback which triggers a new
     *             read() from the pipe.
     *
     * @param[in]  data       pointer to pipe read data buffer
     * @param[in]  bytes      number of bytes read into that buffer
     * @param[out] n_packets  number of valid packets received
     *
     * @return     Returns the same data pointer provided by the first argument, but
     *             cast to an vfc_data_t* struct for convenience. If there was an
     *             error then NULL is returned and n_packets is set to 0
     */
    vfc_data_t *pipe_validate_vfc_data_t(char *data, int bytes, int *n_packets);

////////////////////////////////////////////////////////////////////////////////
// Extended VFC Data (for detailed logging)
////////////////////////////////////////////////////////////////////////////////

#define EXT_VFC_VERSION 8

/**
 * This is the data structure for extended VFC logging, containing comprehensive
 * flight controller state including attitude, position, velocity, and diagnostics.
 */
typedef struct ext_vfc_data_t {
    uint32_t magic_number;      ///< Unique 32-bit number for packet sync

    uint8_t packet_version;
    int64_t timestamp_ns;

    int8_t submode;
    int8_t desired_submode;

    float hover_thrust;
    float thrust_des;
    float roll_des;
    float pitch_des;
    float yaw_des;
    float yaw_rate_des;

    float q0_des;
    float q1_des;
    float q2_des;
    float q3_des;

    float of_x;
    float of_y;
    float of_z;

    float of_x_des;
    float of_y_des;
    float of_z_des;

    float of_vx;
    float of_vy;
    float of_vz;

    float of_vx_des;
    float of_vy_des;
    float of_vz_des;

    float vio_x;
    float vio_y;
    float vio_z;

    float vio_x_des;
    float vio_y_des;
    float vio_z_des;

    float vio_vx;
    float vio_vy;
    float vio_vz;

    float vio_vx_des;
    float vio_vy_des;
    float vio_vz_des;

    uint16_t raw_rc_chans[8];

    bool altitude_ok;
    bool flow_ok;
    bool position_ok;

    bool armed;

    // Backtrack diagnostic log data
    float t0_backtrack;
    bool backtrack_desired;
    bool turtle_mode;
    bool forced_transition_to_offboard;
    int backtrack_storage_index;
    bool backtrack_wraparound;
    int backtrack_data_size;

    // VIO log data
    int32_t vio_quality;
    uint16_t vio_n_feature_points;
    uint8_t vio_state;

    float loop_time; ///< VFC loop time in seconds

} __attribute__((packed)) ext_vfc_data_t;

/**
 * Recommended read buffer size for ext_vfc_data_t.
 */
#define EXT_VFC_RECOMMENDED_READ_BUF_SIZE (sizeof(ext_vfc_data_t) * 10)

/**
 * @brief      Validate ext_vfc_data_t packets from a pipe read.
 *
 * @param[in]  data       pointer to pipe read data buffer
 * @param[in]  bytes      number of bytes read into that buffer
 * @param[out] n_packets  number of valid packets received
 *
 * @return     Returns the data pointer cast to ext_vfc_data_t*, or NULL on error.
 */
ext_vfc_data_t* pipe_validate_ext_vfc_data_t(char* data, int bytes, int* n_packets);

/**
 * @brief      Convert a VFC submode id number to full string name.
 *
 * For example VFC_SM_NONE will return the string "NONE"
 *
 * @param[in]  i     submode id, e.g. VFC_SM_NONE
 *
 * @return     const char* string of the submode
 */
const char* pipe_vfc_submode_to_string(int i);

/**
 * @brief      Convert a VFC submode id number to simplified OSD string.
 *
 * For example VFC_SM_THRUST_ATTITUDE will return "MANUAL"
 *
 * @param[in]  i     submode id, e.g. VFC_SM_THRUST_ATTITUDE
 *
 * @return     const char* simplified string for OSD display
 */
const char* pipe_vfc_submode_to_string_osd(int i);

#ifdef __cplusplus
}
#endif

#endif // VFC_DATA_T_H