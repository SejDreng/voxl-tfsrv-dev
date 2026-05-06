#include "model_helper/piml_model_helper.h"
#include "tensor_data.h"

PIMLModelHelper::PIMLModelHelper(char *model_file, char *labels_file,
                                     DelegateOpt delegate_choice, bool _en_debug,
                                     bool _en_timing, NormalizationType _do_normalize)
    : ModelHelper(model_file, labels_file, delegate_choice, _en_debug, _en_timing, _do_normalize)
{
    // no labels for this model, ignore labels file
}

bool PIMLModelHelper::worker(cv::Mat &output_image, double last_inference_time, camera_image_metadata_t metadata, void *input_params)
{
    if (!postprocess(output_image, last_inference_time, input_params))
        return false;

    // write the output values to the output pipe
    fprintf(stderr, "PIMLModelHelper: writing output (magic=0x%08x, ts=%lld, out=[%.2f, %.2f, %.2f, ...%d values])\n", output_values.magic_number, 
                                                                                                                       (long long)output_values.timestamp_ns, 
                                                                                                                       output_values.output_values[0], 
                                                                                                                       output_values.output_values[1], 
                                                                                                                       output_values.output_values[2], 
                                                                                                                       OUTPUT_BUFFER_SIZE);
    int written = pipe_server_write(REG_OUT_CH, (char *)&output_values, sizeof(piml_output_t));
    if (written < 0) {
        fprintf(stderr, "PIMLModelHelper: pipe_server_write error (%d)\n", written);
    }

    return true;
}

bool PIMLModelHelper::postprocess(cv::Mat &output_image, double last_inference_time, void *input_params)
{
    start_time = rc_nanos_monotonic_time();
    // input params should be a pointer to the input struct with the input values
    if (input_params == nullptr) {
        fprintf(stderr, "Error: input_params is null\n");
        return false;
    }

    piml_input_t *payload = (piml_input_t *)input_params;

    // copy input values to class member for potential use in worker method
    memcpy(input_values.input_values, payload->input_values, sizeof(float) * INPUT_BUFFER_SIZE);

    // run the model inference
    TfLiteTensor *input_tensor = interpreter->tensor(interpreter->inputs()[0]);
    fprintf(stderr, "DEBUG: Input tensor type=%d, dims=%d\n", input_tensor->type, input_tensor->dims->size);
    for (int i = 0; i < INPUT_BUFFER_SIZE; i++) {
        input_tensor->data.f[i] = static_cast<float>(payload->input_values[i]);
    }

    float preprocess_time_ms = (rc_nanos_monotonic_time() - start_time) / 1000000.0f;
    if (!run_inference(output_image, &last_inference_time)) {
        fprintf(stderr, "Error running inference\n");
        return false;
    }

    float postprocess_time_ms_start = rc_nanos_monotonic_time();

    // after inference, copy the output values from the output tensor to the output struct
    TfLiteTensor *output_tensor = interpreter->tensor(interpreter->outputs()[0]);
    fprintf(stderr, "DEBUG: Output tensor type=%d, dims=%d, bytes_size=%d\n", output_tensor->type, output_tensor->dims->size, output_tensor->bytes);
    fprintf(stderr, "DEBUG: First output tensor values (raw): %.6f, %.6f, %.6f\n", output_tensor->data.f[0], output_tensor->data.f[1], output_tensor->data.f[2]);
    for (int i = 0; i < OUTPUT_BUFFER_SIZE; i++) {
        output_values.output_values[i] = static_cast<float>(output_tensor->data.f[i]);
    }

    output_values.magic_number = PIML_OUTPUT_MAGIC_NUMBER;
    output_values.timestamp_ns = rc_nanos_monotonic_time();
    output_values.preprocess_time_ms = preprocess_time_ms;
    output_values.inference_time_ms = last_inference_time;
    output_values.postprocess_time_ms = (rc_nanos_monotonic_time() - postprocess_time_ms_start) / 1000000.0f;

    return true;
}
