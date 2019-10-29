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
 * @brief @c C++ UdfHandle implementation
 * @author Caleb McMillan (caleb.m.mcmillan@intel.com)
 */

#include <vector>
#include <atomic>
#include <iostream>
#include <sstream>
#include <dlfcn.h>
#include <numpy/ndarrayobject.h>
#include <eis/utils/logger.h>
#include "eis/udf/native_udf_handle.h"

#define DELIM ':'

using namespace eis::udf;
using namespace eis::utils;

NativeUdfHandle::NativeUdfHandle(std::string name, int max_workers) : 
	UdfHandle(name, max_workers)
{
	m_lib_handle = NULL;
	m_func_initialize_udf = NULL;
	m_udf = NULL;
} 

NativeUdfHandle::~NativeUdfHandle() {
	LOG_DEBUG_0("Destroying Native UDF");

    m_func_initialize_udf = NULL;
    if(m_udf != NULL){
		delete m_udf;
		m_udf = NULL;
	}
	if(m_lib_handle != NULL)
		dlclose(m_lib_handle);
}

bool NativeUdfHandle::initialize(config_t* config) {
	bool res = this->UdfHandle::initialize(config);
	if(!res)
		return false;
	std::string name = get_name().c_str();
	LOG_DEBUG("Loading native UDF: %s", name);
	
	LOG_DEBUG_0("Retrieving LD_LIBRARY_PATH");
    char* ld_library_path = getenv("LD_LIBRARY_PATH");

    if(ld_library_path == NULL) {
        throw "Failed to retrieve LD_LIBRARY_PATH environmental variable";
    }

    LOG_DEBUG("LD_LIBRARY_PATH: %s", ld_library_path);
    std::stringstream stream(ld_library_path);
    std::string path;

	bool found = false;
	std::ostringstream os;

	os << "lib" << name << ".so";
	std::string lib = os.str();
	os.str("");

	while(std::getline(stream, path, DELIM)) {
        if(path.empty())
            continue;
        os << path << "/" << lib;
        LOG_DEBUG("Checking if '%s' exists", os.str().c_str());
        if(access(os.str().c_str(), F_OK) != -1) {
            found = true;
            lib = os.str();
            break;
        }
        os.str("");
    }

    if(found) {
        LOG_DEBUG("Found native UDF: %s", lib.c_str());
		m_lib_handle = dlopen(lib.c_str(), RTLD_LAZY);
		
		if(!m_lib_handle) {
			LOG_ERROR("Failed to load UDF library: %s", dlerror());
			return false;
		} else {
			LOG_DEBUG_0("Successfully loaded UDF library");
			*(void**)(&m_func_initialize_udf) = dlsym(m_lib_handle, "initialize_udf");
	
			if(!m_func_initialize_udf) {
				LOG_ERROR("Failed to find initialize_udf symbol: %s", dlerror());
				dlclose(m_lib_handle);
				return false;
			}
			LOG_DEBUG_0("Successfully found initialize_udf symbol");
		}
		
		void* udf = m_func_initialize_udf();
		m_udf = (BaseUdf*) udf;
	
		if(!m_udf->initialize(config)) {
			LOG_ERROR_0("Failed to initialize UDF");
			return false;
		}
		else
			LOG_DEBUG_0("Successfully initialized UDF");

        return true;
    }

	return false;
}

UdfRetCode NativeUdfHandle::process(Frame* frame) {
	int w = frame->get_width();
	int h = frame->get_height();
	int c = frame->get_channels();
	//TODO: Do we want to default to 8bit?
	cv::Mat* mat_frame = new cv::Mat(h, w, CV_8UC(c), frame->get_data());
	msg_envelope_t* meta_data = frame->get_meta_data();
	
	UdfRetCode ret = m_udf->process(mat_frame, meta_data);
	delete mat_frame;
	mat_frame = NULL;
	if (ret == UdfRetCode::UDF_ERROR)
		LOG_ERROR_0("Error in UDF process() method");

	return ret;
}


