#include "config_file.h"
#include "utils.h"
#include "model_helper/model_info.h"
#include "inference_handler.h"

#define PROCESS_NAME "voxl-tflite-server"
#define HIRES_PIPE "/run/mpa/hires_small_color/"
#define TFLITE_IMAGE_PATH (MODAL_PIPE_DEFAULT_BASE_DIR "tflite/")
#define TFLITE_DETECTION_PATH (MODAL_PIPE_DEFAULT_BASE_DIR "tflite_data/")


// DEFINE NEW PIPES FOR REGRESSION MODE (just configure to use tflite_data for output and ignore image output for regression models for now, configured in if statement in main)
#define REG_IN_PIPE "/run/mpa/reg_in/"
#define REG_OUT_PIPE "/run/mpa/reg_out/"

const int CAMERA_CLIENT_CHANNEL = pipe_client_get_next_available_channel();
ModelHelper *model_helper;
static ModelName regression_model_name = PLACEHOLDER;
static size_t regression_input_bytes = 0;
static size_t regression_output_bytes = 0;

bool en_debug = false;
bool en_timing = false;
char *coco_labels = (char *)"/usr/bin/dnn/coco_labels.txt";
char *city_labels = (char *)"/usr/bin/dnn/cityscapes_labels.txt";
char *imagenet_labels = (char *)"/usr/bin/dnn/imagenet_labels.txt";
char *yolo_labels = (char *)"/usr/bin/dnn/yolov5_labels.txt";

static void _camera_helper_cb(__attribute__((unused)) int ch,
                              camera_image_metadata_t meta, char *frame,
                              void *context);
static void _camera_disconnect_cb(__attribute__((unused)) int ch,
                                  __attribute__((unused)) void *context);
static void _camera_connect_cb(__attribute__((unused)) int ch,
                               __attribute__((unused)) void *context);
static void set_delegate(DelegateOpt *opt);
static void get_model_type(char *model, ModelName *model_name, ModelCategory *model_category);
static void get_normalization_type(char *model, NormalizationType *norm_type);
static size_t get_regression_input_bytes(ModelName model_name);
static size_t get_regression_output_bytes(ModelName model_name);

static void control_pipe_handler(int ch, char* string, int bytes, __attribute__((unused)) void* context)
{
	printf("received command on channel %d bytes: %d string: \"%s\"\n", ch, bytes, string);

	if (strncmp(string, "set_cam", 7) == 0) {

		std::string command (string);
		std::string new_input_pipe (command.substr(command.rfind(" ", command.size() - 2) + 1));
		strncpy(input_pipe, new_input_pipe.c_str(), sizeof(input_pipe) - 1);
	
		pipe_client_close(CAMERA_CLIENT_CHANNEL);
		if (pipe_client_open(CAMERA_CLIENT_CHANNEL, input_pipe, PROCESS_NAME,
							CLIENT_FLAG_EN_CAMERA_HELPER, 0))
		{
			fprintf(stderr, "Failed to open pipe: %s\n", input_pipe);
			exit(-1);
		}
	}
	return;
}

// Patch with the input pipe being a non-camera pipe for regression models
static void non_camera_input_pipe_handler(int ch, char* data, int bytes, __attribute__((unused)) void* context)
{
    if (model_helper == nullptr) {
        fprintf(stderr, "non_camera_input_pipe_handler: model_helper is NULL\n");
        return;
    }

    if (regression_input_bytes == 0) {
        fprintf(stderr, "non_camera_input_pipe_handler: regression input size not configured\n");
        return;
    }

    if (bytes != (int)regression_input_bytes) {
        fprintf(stderr, "ERROR: Expected %zu bytes, got %d\n", regression_input_bytes, bytes);
        return;
    }

    fprintf(stderr, "non_camera_input_pipe_handler: received %d bytes on ch %d\n", bytes, ch);

    cv::Mat dummy_output;
    camera_image_metadata_t dummy_metadata = {};

    if (regression_model_name == XOR) {
        xor_input_t *payload = (xor_input_t *)data;
        fprintf(stderr, "non_camera_input_pipe_handler: inputs = %d, %d\n", payload->input_values[0], payload->input_values[1]);
        if (!model_helper->worker(dummy_output, 0.0, dummy_metadata, (void *)payload)) {
            fprintf(stderr, "non_camera_input_pipe_handler: model_helper->worker returned false\n");
        }
    }
    else if (regression_model_name == PIML) {
        piml_input_t *payload = (piml_input_t *)data;
        fprintf(stderr, "non_camera_input_pipe_handler: first inputs = %f, %f\n", payload->input_values[0], payload->input_values[1]);
        if (!model_helper->worker(dummy_output, 0.0, dummy_metadata, (void *)payload)) {
            fprintf(stderr, "non_camera_input_pipe_handler: model_helper->worker returned false\n");
        }
    }
    else {
        fprintf(stderr, "non_camera_input_pipe_handler: unsupported regression model\n");
        return;
    }

    return;
}

int main(int argc, char *argv[])
{
    if (_parse_opts(argc, argv, &en_debug, &en_timing))
        return -1;

    ////////////////////////////////////////////////////////////////////////////////
    // load config, need to do before checking for other instances
    ////////////////////////////////////////////////////////////////////////////////
    if (config_file_read())
        return -1;

    config_file_print();

    if (!allow_multiple)
    {
        ////////////////////////////////////////////////////////////////////////////////
        // gracefully handle an existing instance of the process and associated
        // PID file
        ////////////////////////////////////////////////////////////////////////////////

        // make sure another instance isn't running
        // if return value is -3 then a background process is running with
        // higher privaledges and we couldn't kill it, in which case we should
        // not continue or there may be hardware conflicts. If it returned -4
        // then there was an invalid argument that needs to be fixed.
        if (kill_existing_process(PROCESS_NAME, 2.0) < -2)
            return -1;

        // start signal handler so we can exit cleanly
        if (enable_signal_handler() == -1)
        {
            fprintf(stderr, "ERROR: failed to start signal handler\n");
            return (-1);
        }

        // make PID file to indicate your project is running
        // due to the check made on the call to rc_kill_existing_process() above
        // we can be fairly confident there is no PID file already and we can
        // make our own safely.
        make_pid_file(PROCESS_NAME);
    }

    // disable garbage multithreading
    cv::setNumThreads(1);

    ModelName model_name;
    ModelCategory model_category;
    DelegateOpt opt_;
    NormalizationType do_normalize = NONE;

    ////////////////////////////////////////////////////////////////////////////////
    // initialize InferenceHelper
    ////////////////////////////////////////////////////////////////////////////////

    set_delegate(&opt_);
    get_model_type(model, &model_name, &model_category);
    get_normalization_type(model, &do_normalize);

    regression_model_name = model_name;
    regression_input_bytes = get_regression_input_bytes(model_name);
    regression_output_bytes = get_regression_output_bytes(model_name);

    model_helper = create_model_helper(model_name, model_category, opt_, do_normalize);

    InferenceWorkerArgs *args = nullptr;

    if(model_category != REGRESSION) {

        // store cam name
        std::string full_path(input_pipe);
        std::string cam_name(
            full_path.substr(full_path.rfind("/", full_path.size() - 2) + 1));
        cam_name.pop_back();

        model_helper->cam_name = cam_name;

        main_running = 1;

        fprintf(stderr, "\n------VOXL TFLite Server------\n\n");

        // Start the thread that will run the tensorflow lite model on live camera
        // frames.
        pthread_attr_t thread_attributes;
        pthread_attr_init(&thread_attributes);
        pthread_attr_setdetachstate(&thread_attributes, PTHREAD_CREATE_JOINABLE);

        args = new InferenceWorkerArgs;
        args->model_helper = model_helper;

        int ret = pthread_create(&(model_helper->thread), &thread_attributes, run_inference_pipeline, args);
        if (ret != 0)
        {
            fprintf(stderr, "Error creating inference worker thread: %d\n", ret);
            delete args; // Clean up in case of failure
        }

        // fire up our camera server connection

        pipe_client_set_connect_cb(CAMERA_CLIENT_CHANNEL, _camera_connect_cb, NULL);
        pipe_client_set_disconnect_cb(CAMERA_CLIENT_CHANNEL, _camera_disconnect_cb, NULL);
        pipe_client_set_camera_helper_cb(CAMERA_CLIENT_CHANNEL, _camera_helper_cb, NULL);

        if (pipe_client_open(CAMERA_CLIENT_CHANNEL, input_pipe, PROCESS_NAME,
                            CLIENT_FLAG_EN_CAMERA_HELPER, 0))
        {
            fprintf(stderr, "Failed to open pipe: %s\n", input_pipe);
            return -1;
        }

        if (!allow_multiple)
        {
            // open our output pipes using default names
            pipe_info_t image_pipe = {
                "tflite", TFLITE_IMAGE_PATH, "camera_image_metadata_t",
                PROCESS_NAME, 16 * 1024 * 1024, 0};
            pipe_server_create(IMAGE_CH, image_pipe, 0);
            if (model_category == OBJECT_DETECTION)
            {
                pipe_info_t detection_pipe = {
                    "tflite_data", TFLITE_DETECTION_PATH,
                    "ai_detection_t", PROCESS_NAME,
                    16 * 1024, 0};

                    char cont_cmds[256];
                    snprintf(cont_cmds, 255, "%s",
                        "set_cam");
                    pipe_server_set_control_cb(DETECTION_CH, &control_pipe_handler, NULL);
                    pipe_server_create(DETECTION_CH, detection_pipe, SERVER_FLAG_EN_CONTROL_PIPE);
                    pipe_server_set_available_control_commands(DETECTION_CH, cont_cmds);
            }
        }
        else
        {
            // set up a string to hold our custom pipe name
            std::string output_pipe_holder = MODAL_PIPE_DEFAULT_BASE_DIR;
            output_pipe_holder.append(output_pipe_prefix);
            output_pipe_holder.append("_tflite");

            // get c ptr to string
            const char *buf_ptr = output_pipe_holder.c_str();

            // create our output pipe
            pipe_info_t image_pipe = {
                "tflite", "unknown", "camera_image_metadata_t",
                PROCESS_NAME, 16 * 1024 * 1024, 0};
            // set the location to our new strings ptr
            memcpy(image_pipe.location, buf_ptr, MODAL_PIPE_MAX_NAME_LEN);
            // create the server pipe
            pipe_server_create(IMAGE_CH, image_pipe, 0);

            if (model_category == OBJECT_DETECTION)
            {
                // initialize the detection pipe only if we are running a detection
                // model
                pipe_info_t detection_pipe = {"tflite_data", "unknown",
                                            "ai_detection_t", PROCESS_NAME,
                                            16 * 1024, 0};
                output_pipe_holder = MODAL_PIPE_DEFAULT_BASE_DIR;
                output_pipe_holder.append(output_pipe_prefix);
                output_pipe_holder.append("_tflite_data");

                buf_ptr = output_pipe_holder.c_str();

                memcpy(detection_pipe.location, buf_ptr, MODAL_PIPE_MAX_NAME_LEN);
                
                char cont_cmds[256];
                snprintf(cont_cmds, 255, "%s",
                    "set_cam");

                pipe_server_set_control_cb(DETECTION_CH, &control_pipe_handler, NULL);
                pipe_server_create(DETECTION_CH, detection_pipe, SERVER_FLAG_EN_CONTROL_PIPE);
                pipe_server_set_available_control_commands(DETECTION_CH, cont_cmds);
            }
        }
    }
    else {
        fprintf(stderr, "Running in regression mode, no camera input will be processed\n");
        if(model_helper == nullptr) {
            fprintf(stderr, "Failed to create model helper for regression mode\n");
            return -1;
        }

        model_helper->cam_name = "regression";

        main_running = 1;

        fprintf(stderr, "\n------VOXL TFLite Server------\n\n");

        pthread_attr_t thread_attributes;
        pthread_attr_init(&thread_attributes);
        pthread_attr_setdetachstate(&thread_attributes, PTHREAD_CREATE_JOINABLE);

        args = new InferenceWorkerArgs;
        args->model_helper = model_helper;

        int ret = pthread_create(&(model_helper->thread), &thread_attributes,
                                run_inference_pipeline, args);
        if (ret != 0) {
            fprintf(stderr, "Error creating inference worker thread: %d\n", ret);
            delete args;
            return -1;
        }

        // If regression input comes from another pipe, create/open that here.
        // Do NOT use the camera helper callback.
        // Use a regression-specific callback and queue instead.
        if (model_name == XOR) {
            pipe_info_t reg_pipe_in = {"reg_in", REG_IN_PIPE, "xor_input_t", PROCESS_NAME, (int)(regression_input_bytes * 16), 0};
            if (pipe_server_create(REG_IN_CH, reg_pipe_in, 0)) {
                fprintf(stderr, "Failed to create regression input pipe: %s\n", REG_IN_PIPE);
            }

            pipe_info_t reg_pipe_out = {"reg_out", REG_OUT_PIPE, "xor_output_t", PROCESS_NAME, (int)(regression_output_bytes * 16), 0};
            if (pipe_server_create(REG_OUT_CH, reg_pipe_out, 0)) {
                fprintf(stderr, "Failed to create regression output pipe: %s\n", REG_OUT_PIPE);
            }

            // Open the regression input pipe as a client with a simple helper callback
            // This will receive xor_input_t payloads and feed them to the model
            pipe_client_set_simple_helper_cb(REG_IN_CH, non_camera_input_pipe_handler, NULL);
            if (pipe_client_open(REG_IN_CH, REG_IN_PIPE, PROCESS_NAME, CLIENT_FLAG_EN_SIMPLE_HELPER, (int)regression_input_bytes)) {
                fprintf(stderr, "Failed to open regression input pipe as client\n");
            }
        }
        else if (model_name == PIML) {
            pipe_info_t reg_pipe_in = {"reg_in", REG_IN_PIPE, "piml_input_t", PROCESS_NAME, (int)(regression_input_bytes * 16), 0};
            if (pipe_server_create(REG_IN_CH, reg_pipe_in, 0)) {
                fprintf(stderr, "Failed to create regression input pipe: %s\n", REG_IN_PIPE);
            }

            pipe_info_t reg_pipe_out = {"reg_out", REG_OUT_PIPE, "piml_output_t", PROCESS_NAME, (int)(regression_output_bytes * 16), 0};
            if (pipe_server_create(REG_OUT_CH, reg_pipe_out, 0)) {
                fprintf(stderr, "Failed to create regression output pipe: %s\n", REG_OUT_PIPE);
            }

            // Open the regression input pipe as a client with a simple helper callback
            // This will receive piml_input_t payloads and feed them to the model
            pipe_client_set_simple_helper_cb(REG_IN_CH, non_camera_input_pipe_handler, NULL);
            if (pipe_client_open(REG_IN_CH, REG_IN_PIPE, PROCESS_NAME, CLIENT_FLAG_EN_SIMPLE_HELPER, (int)regression_input_bytes)) {
                fprintf(stderr, "Failed to open regression input pipe as client\n");
            }
        }
    }    

    while (main_running)
    {
        usleep(5000000);
    }

    pipe_client_close_all();
    pipe_server_close_all();

    fprintf(stderr, "\nStopping the application\n");

    model_helper->cond_var.notify_all();
    pthread_join(model_helper->thread, NULL);

    delete (args);
    delete (model_helper);
    return 0;
}

// offset for classification models only (as of now), used for addressing same
// tensor data with varied offsets i.e. efficient net has 0 offset [0,1000],
// mobilenetv1 has +1 offset [1, 1001], etc...

static void _camera_connect_cb(__attribute__((unused)) int ch,
                               __attribute__((unused)) void *context)
{
    printf("Connected to camera server\n");
}

static void _camera_disconnect_cb(__attribute__((unused)) int ch,
                                  __attribute__((unused)) void *context)
{
    fprintf(stderr, "Disconnected from camera server\n");
}

static void _camera_helper_cb(__attribute__((unused)) int ch,
                              camera_image_metadata_t meta, char *frame,
                              void *context)
{
    static int n_skipped = 0;
    if (n_skipped < skip_n_frames)
    {
        n_skipped++;
        return;
    }
    else
        n_skipped = 0;

    if (pipe_client_bytes_in_pipe(ch) > 0)
    {
        n_skipped++;
        if (en_debug)
            fprintf(
                stderr,
                "WARNING, skipping frame on channel %d due to frame backup\n",
                ch);
        return;
    }
    if (!en_debug && !en_timing)
    {
        if (!pipe_server_get_num_clients(IMAGE_CH) &&
            !pipe_server_get_num_clients(DETECTION_CH))
            return;
    }

    if (meta.size_bytes > MAX_IMAGE_SIZE)
    {
        fprintf(stderr, "Model cannot process an image with %d bytes\n",
                meta.size_bytes);
        return;
    }

    int queue_ind = model_helper->camera_queue.insert_idx;

    TFLiteMessage *camera_message = &model_helper->camera_queue.queue[queue_ind];

    camera_message->metadata = meta;
    memcpy(camera_message->image_pixels, (uint8_t *)frame, meta.size_bytes);

    model_helper->camera_queue.insert_idx = ((queue_ind + 1) % QUEUE_SIZE);
    model_helper->cond_var.notify_all();

    // print timing if requested
    if (en_timing)
        model_helper->print_summary_stats();

    

    return;
}

static void set_delegate(DelegateOpt *opt)
{
    *opt = GPU; // default for MAI models
    if (!strcmp(delegate, "cpu"))
        *opt = XNNPACK;
    else if (!strcmp(delegate, "nnapi"))
        *opt = NNAPI;
}

static void get_model_type(char *model, ModelName *model_name, ModelCategory *model_category)
{
    if (!strcasecmp(model_architecture, "MOBILE_NET"))
    {
        *model_name = MOBILE_NET;
        *model_category = OBJECT_DETECTION;
    }
    else if (!strcasecmp(model_architecture, "MOBILE_NET_CLASSIFIER"))
    {
        *model_name = MOBILE_NET;
        *model_category = CLASSIFICATION;
    }
    else if (!strcasecmp(model_architecture, "YOLOV5"))
    {
        *model_name = YOLOV5;
        *model_category = OBJECT_DETECTION;
    }
    else if (!strcasecmp(model_architecture, "YOLOV8"))
    {
        *model_name = YOLOV8;
        *model_category = OBJECT_DETECTION;
    }
    else if (!strcasecmp(model_architecture, "YOLOV11"))
    {
        *model_name = YOLOV11;
        *model_category = OBJECT_DETECTION;
    }
    else if (!strcasecmp(model_architecture, "EFFICIENT_NET"))
    {
        *model_name = EFFICIENT_NET;
        *model_category = CLASSIFICATION;
    }
    else if (!strcasecmp(model_architecture, "POSENET"))
    {
        *model_name = POSENET;
        *model_category = POSE;
    }
    else if (!strcasecmp(model_architecture, "FAST_DEPTH"))
    {
        *model_name = FAST_DEPTH;
        *model_category = MONO_DEPTH;
    }
    else if (!strcasecmp(model_architecture, "DEEPLAB"))
    {
        *model_name = DEEPLAB;
        *model_category = SEGMENTATION;
    }
    else if (!strcasecmp(model_architecture, "XOR"))
    {
        *model_name = XOR;
        *model_category = REGRESSION;
    }
    else if (!strcasecmp(model_architecture, "PIML"))
    {
        *model_name = PIML;
        *model_category = REGRESSION;
    }
    else
    {
        fprintf(stderr,
                "ERROR: Unknown model_architecture '%s'\n"
                "Valid options: MOBILE_NET, MOBILE_NET_CLASSIFIER, YOLOV5, YOLOV8, YOLOV11, EFFICIENT_NET, POSENET, FAST_DEPTH, DEEPLAB, XOR, PIML\n",
                model_architecture);
        fprintf(stderr, "Failed to parse model_architecture from config\n");
        exit(-1);
    }
}

static void get_normalization_type(char *model, NormalizationType *norm_type_out)
{
    if (!strcasecmp(norm_type, "PIXEL_MEAN"))
    {
        *norm_type_out = PIXEL_MEAN;
    }
    else if (!strcasecmp(norm_type, "HARD_DIVISION"))
    {
        *norm_type_out = HARD_DIVISION;
    }
    else if (!strcasecmp(norm_type, "NONE"))
    {
        *norm_type_out = NONE;
    }
    else
    {
        fprintf(stderr,
                "ERROR: Unknown norm_type '%s'\n"
                "Valid options: PIXEL_MEAN, HARD_DIVISION, NONE\n",
                norm_type);
        fprintf(stderr, "Failed to parse norm_type from config\n");
        exit(-1);
    }
}


// Add new model I/O structs here
static size_t get_regression_input_bytes(ModelName model_name)
{
    switch (model_name)
    {
    case XOR:
        return sizeof(xor_input_t);
    case PIML:
        return sizeof(piml_input_t);
    default:
        return 0;
    }
}

static size_t get_regression_output_bytes(ModelName model_name)
{
    switch (model_name)
    {
    case XOR:
        return sizeof(xor_output_t);
    case PIML:
        return sizeof(piml_output_t);
    default:
        return 0;
    }
}