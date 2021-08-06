// Copyright (c) 2021 Intel Corporation.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM,OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

/**
 * @file
 * @brief Raw Native UDF Implementation for unit tests
 */

#include <eii/udf/raw_base_udf.h>
#include <eii/utils/logger.h>
#include <opencv2/opencv.hpp>
#include <iostream>

using namespace eii::udf;

#define GET_CONFIG_BOOL(config, key, dest) { \
    config_value_t* value = config_get(config, key); \
    if (value == NULL) { throw "Failed to get config value "#key; } \
    if (value->type != CVT_BOOLEAN) { \
        config_value_destroy(value); \
        throw #key" must be a boolean value"; \
    } \
    dest = value->body.boolean; \
    config_value_destroy(value); \
}

namespace eii {
namespace udftests {

void free_cv_frame(void* varg) {
    cv::Mat* mat = (cv::Mat*) varg;
    delete mat;
}

class UnitTestRawNativeUdf : public RawBaseUdf {
private:
    // Return the input frame as the output frame
    bool m_same_frame;

    // Resize the input frame and return as the output
    bool m_resize;

public:
    UnitTestRawNativeUdf(config_t* config) : RawBaseUdf(config) {
        GET_CONFIG_BOOL(config, "same_frame", m_same_frame);
        GET_CONFIG_BOOL(config, "resize", m_resize);

        // Can only return the same frame OR resize
        assert((!m_same_frame && m_resize) || (m_same_frame && !m_resize));
    };

    ~UnitTestRawNativeUdf() {};

    UdfRetCode process(Frame* frame) override {
        if (m_same_frame) {
            // Return as-is, there is no "same-fame"
            return UdfRetCode::UDF_OK;
        } else {
            // Resize any given frames
            for (int i = 0; i < frame->get_number_of_frames(); i++) {
                int w = frame->get_width(i);
                int h = frame->get_height(i);
                int c = frame->get_channels(i);
                cv::Mat img(h, w, CV_8UC(c), frame->get_data(i));
                cv::Mat* out = new cv::Mat();
                cv::resize(img, *out, cv::Size(100, 100));
                frame->set_data(
                        i, (void*) out, free_cv_frame, (void*) out->data,
                        out->cols, out->rows, out->channels());
            }
        }

        return UdfRetCode::UDF_OK;
    };
};
} // udf
} // eii

extern "C" {

/**
 * Create the UDF.
 *
 * @return void*
 */
void* initialize_udf(config_t* config) {
    eii::udftests::UnitTestRawNativeUdf* udf =
        new eii::udftests::UnitTestRawNativeUdf(config);
    return (void*) udf;
}

} // extern "C"

