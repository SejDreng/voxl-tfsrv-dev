#include "model_helper/xor_model_helper.h"
#include "tensor_data.h"

XORModelHelper::XORModelHelper(char *model_file, char *labels_file,
                                     DelegateOpt delegate_choice, bool _en_debug,
                                     bool _en_timing, NormalizationType _do_normalize)
    : ModelHelper(model_file, labels_file, delegate_choice, _en_debug, _en_timing, _do_normalize)
{
    // no labels for this model, ignore labels file
}

bool XORModelHelper::worker(cv::Mat &output_image, double last_inference_time, camera_image_metadata_t metadata, void *input_params)
{
    if (!postprocess(output_image, last_inference_time, input_params))
        return false;

    // write the output values to the output pipe
    fprintf(stderr, "XORModelHelper: writing output (magic=0x%08x, ts=%lld, out=%d)\n", output_values.magic_number, (long long)output_values.timestamp_ns, output_values.output_value);
    int written = pipe_server_write(REG_OUT_CH, (char *)&output_values, sizeof(xor_output_t));
    if (written < 0) {
        fprintf(stderr, "XORModelHelper: pipe_server_write error (%d)\n", written);
    }

    return true;
}

bool XORModelHelper::postprocess(cv::Mat &output_image, double last_inference_time, void *input_params)
{
    start_time = rc_nanos_monotonic_time();
    // input params should be a pointer to the input struct with the input values
    if (input_params == nullptr) {
        fprintf(stderr, "Error: input_params is null\n");
        return false;
    }

    xor_input_t *payload = (xor_input_t *)input_params;

    // copy input values to class member for potential use in worker method
    memcpy(input_values.input_values, payload->input_values, sizeof(int) * INPUT_BUFFER_SIZE);

    float preprocess_time_ms = (rc_nanos_monotonic_time() - start_time) / 1000000.0f;

    // run the model inference
    TfLiteTensor *input_tensor = interpreter->tensor(interpreter->inputs()[0]);
    for (int i = 0; i < INPUT_BUFFER_SIZE; i++) {
        input_tensor->data.f[i] = static_cast<float>(payload->input_values[i]);
    }

    if (!run_inference(output_image, &last_inference_time)) {
        fprintf(stderr, "Error running inference\n");
        return false;
    }

    float postprocess_time_ms_start = rc_nanos_monotonic_time();


    // after inference, copy the output values from the output tensor to the output struct
    TfLiteTensor *output_tensor = interpreter->tensor(interpreter->outputs()[0]);
    output_values.magic_number = XOR_OUTPUT_MAGIC_NUMBER;
    output_values.timestamp_ns = rc_nanos_monotonic_time();
    output_values.output_value = static_cast<int>(output_tensor->data.f[0]);
    output_values.preprocess_time_ms = preprocess_time_ms;
    output_values.inference_time_ms = last_inference_time;
    output_values.postprocess_time_ms = (rc_nanos_monotonic_time() - postprocess_time_ms_start) / 1000000.0f;

    return true;
}
