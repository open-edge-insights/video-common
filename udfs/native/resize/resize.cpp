// Copyright (c) 2019 Intel Corporation.
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
 * @brief Resize UDF Implementation
 */

#include <eis/udf/base_udf.h>
#include <eis/utils/logger.h>
#include <iostream>
#include <opencv2/opencv.hpp>

using namespace eis::udf;

namespace eis {
    namespace udfsamples {

    /**
     * The Resize UDF
     */
        class ResizeUdf : public BaseUdf {
            private:
                int m_width;
                int m_height;

            public:
                ResizeUdf(config_t* config): BaseUdf(config) {};

                ~ResizeUdf() {};

                UdfRetCode process(cv::Mat& frame, cv::Mat& output, msg_envelope_t* meta) override {
                    config_value_t* width = m_config->get_config_value(m_config->cfg,"width");
                    if(width == NULL) {
                        throw "Failed to get width";
                    }
                    if(width->type != CVT_INTEGER) {
                        throw "width must be a string";
                    }

                    config_value_t* height = m_config->get_config_value(m_config->cfg,"height");

                    if(height == NULL) {
                    throw "Failed to get height";
                    }
                    if(height->type != CVT_INTEGER) {
                        throw "height must be a string";
                    }

                    m_width = width->body.integer;
                    m_height = height->body.integer;

                    cv::resize(frame,output, cv::Size(m_width,m_height));

                    msgbus_ret_t ret = MSG_SUCCESS;

                    // Add width
                    msg_envelope_elem_body_t* e_width = msgbus_msg_envelope_new_integer(m_width);
                    if(e_width == NULL) {
                        LOG_ERROR_0("Failed to initialize width meta-data");
                    }
                    ret = msgbus_msg_envelope_put(meta, "resize_width", e_width);
                    if(ret != MSG_SUCCESS) {
                        LOG_ERROR_0("Failed to put width meta-data");
                        return UdfRetCode::UDF_ERROR;
                    }

                    // Add height
                    msg_envelope_elem_body_t* e_height = msgbus_msg_envelope_new_integer(m_height);
                    if(e_height == NULL) {
                        LOG_ERROR_0("Failed to initialize height meta-data");
                    }
                    ret = msgbus_msg_envelope_put(meta, "resize_height", e_height);
                    if(ret != MSG_SUCCESS) {
                        LOG_ERROR_0("Failed to put height meta-data");
                        msgbus_msg_envelope_elem_destroy(e_width);
                        return UdfRetCode::UDF_ERROR;
                    }

                    return UdfRetCode::UDF_OK;
            };
        };
    } // udf
} // eis

extern "C" {

/**
 * Create the UDF.
 *
 * @return void*
 */
void* initialize_udf(config_t* config) {
    eis::udfsamples::ResizeUdf* udf = new eis::udfsamples::ResizeUdf(config);
    return (void*) udf;
}

} // extern "C"

