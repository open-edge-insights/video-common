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
 * @brief Dummy UDF Implementation
 */

#include <eii/udf/raw_base_udf.h>
#include <eii/utils/logger.h>
#include <iostream>

using namespace eii::udf;

namespace eii {
namespace udfsamples {

/**
 * The Raw Dummy UDF - does no processing
 */
class RawDummyUdf : public RawBaseUdf {
public:
    explicit RawDummyUdf(config_t* config) : RawBaseUdf(config) {};

    ~RawDummyUdf() {};

    UdfRetCode process(Frame* frame) override {
        LOG_INFO("Received frame with %d frames",
                frame->get_number_of_frames());
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
    eii::udfsamples::RawDummyUdf* udf = new eii::udfsamples::RawDummyUdf(config);
    return (void*) udf;
}

} // extern "C"

