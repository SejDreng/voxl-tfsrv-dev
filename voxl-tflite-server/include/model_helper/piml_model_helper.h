#ifndef PIML_H
#define PIML_H

#include "model_helper/model_helper.h"

class PIMLModelHelper : public ModelHelper {
    public:
        PIMLModelHelper(char *model_file, char *labels_file,
                       DelegateOpt delegate_choice, bool _en_debug,
                       bool _en_timing, NormalizationType _do_normalize);
        bool postprocess(cv::Mat &output_image, double last_inference_time, void *input_params) override;
        bool worker(cv::Mat &output_image, double last_inference_time, camera_image_metadata_t metadata, void *input_params) override;
    private:
        piml_input_t input_values;
        piml_output_t output_values;
};

#endif