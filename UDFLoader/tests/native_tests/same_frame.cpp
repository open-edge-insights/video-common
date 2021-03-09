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
 * @brief Same File UDF Implementation
 */

#include <eii/udf/base_udf.h>
#include <eii/utils/logger.h>
#include <iostream>

using namespace eii::udf;

namespace eii {
namespace udftests {

/**
* The Same Frame UDF - Does not do any processing on the given frames, but
* returns as the "outputted frame" the same frame it receieved to process.
*/
class SameFrameUdf : public BaseUdf {
public:
    /**
     * Constructor
     *
     * @param config - UDF configuration
     */
    SameFrameUdf(config_t* config) : BaseUdf(config) {};

    /**
     * Destructor
     */
    ~SameFrameUdf() {};

    UdfRetCode process(
            cv::Mat& frame, cv::Mat& output, msg_envelope_t* meta) override {
        LOG_DEBUG("In %s method...", __PRETTY_FUNCTION__);

        // Assign the output to the same memory as the input frame, this is
        // to make sure that the UDF Loader/Manager does not accidentally
        // free the "output" memory thinking it is a new frame
        output = frame;

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
    eii::udftests::SameFrameUdf* udf = new eii::udftests::SameFrameUdf(config);
    return (void*) udf;
}

}  // extern "C"

