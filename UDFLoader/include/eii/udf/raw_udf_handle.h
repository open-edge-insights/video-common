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
 * @brief C++ UDF to process raw UDF Manager @c Frame objects
 */

#ifndef _EII_UDF_RAW_UDF_H
#define _EII_UDF_RAW_UDF_H


#include "eii/udf/udf_handle.h"
#include "eii/udf/raw_base_udf.h"

namespace eii {
namespace udf {

class RawUdfHandle : public UdfHandle {
private:
	//References needed after init
	void* m_lib_handle;
	void* (*m_func_initialize_udf)(config_t*);
	RawBaseUdf* m_udf;

    /**
     * Private @c RawUdfHandle copy constructor.
     */
    RawUdfHandle(const RawUdfHandle& src);

    /**
     * Private @c NativeUdfHandle assignment operator.
     *
     */
    RawUdfHandle& operator=(const RawUdfHandle& src);

public:
    /**
     * Constructor
     *
     * @param name - Name of the Native UDF
     */
    RawUdfHandle(std::string name, int max_workers);

    /**
     * Destructor
     */
    ~RawUdfHandle();

    /**
     * Overridden initialization method
     *
     * @param config - UDF configuration
     * @return bool
     */
    bool initialize(config_t* config) override;

    /**
     * Overridden frame processing method.
     *
     * @param frame - Frame to process
     * @return UdfRetCode
     */
    UdfRetCode process(Frame* frame) override;
};

} // eii
} // udf

#endif // _EII_UDF_RAW_UDF_H
