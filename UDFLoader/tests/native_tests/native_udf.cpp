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
 * @brief Native UDF Implementation for unit tests
 */

#include <eii/udf/base_udf.h>
#include <eii/utils/logger.h>
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

class UnitTestNativeUdf : public BaseUdf {
private:
    // Return the input frame as the output frame
    bool m_same_frame;

    // Resize the input frame and return as the output
    bool m_resize;

public:
    /**
     * Constructor
     *
     * @param config - UDF configuration
     */
    UnitTestNativeUdf(config_t* config) : BaseUdf(config) {
        GET_CONFIG_BOOL(config, "same_frame", m_same_frame);
        GET_CONFIG_BOOL(config, "resize", m_resize);

        // Can only return the same frame OR resize
        assert((!m_same_frame && m_resize) || (m_same_frame && !m_resize));
    };

    /**
     * Destructor
     */
    ~UnitTestNativeUdf() {};

    UdfRetCode process(
            cv::Mat& frame, cv::Mat& output, msg_envelope_t* meta) override {
        LOG_DEBUG("In %s method...", __PRETTY_FUNCTION__);

        if (m_resize) {
            cv::resize(frame, output, cv::Size(100, 100));
        } else if (m_same_frame) {
            output = frame;
        }

        return UdfRetCode::UDF_OK;
    };
};
}  // namespace udftests
}  // namespace eii

extern "C" {

/**
 * Create the UDF.
 *
 * @return void*
 */
void* initialize_udf(config_t* config) {
    eii::udftests::UnitTestNativeUdf* udf =
        new eii::udftests::UnitTestNativeUdf(config);
    return (void*) udf;
}

}  // extern "C"
