#ifndef XOR_H
#define XOR_H

#include "model_helper/model_helper.h"

class XORModelHelper : public ModelHelper {
    public:
        XORModelHelper(char *model_file, char *labels_file,
                       DelegateOpt delegate_choice, bool _en_debug,
                       bool _en_timing, NormalizationType _do_normalize);
        bool postprocess(cv::Mat &output_image, double last_inference_time, void *input_params) override;
        bool worker(cv::Mat &output_image, double last_inference_time, camera_image_metadata_t metadata, void *input_params) override;
    private:
        xor_input_t input_values;
        xor_output_t output_values;
};

#endif