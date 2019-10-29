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
 * @brief Implementation of @c PythonUdfHandle object.
 * @author Kevin Midkiff (kevin.midkiff@intel.com)
 */

// Defining NumPy version
#define NPY_NO_DEPRECATED_API NPY_1_14_API_VERSION

#include <vector>
#include <atomic>
#include <mutex>
#include <numpy/ndarrayobject.h>

#include <eis/utils/logger.h>
#include "eis/udf/python_udf_handle.h"
#include "cython/udf.h"

using namespace eis::udf;
using namespace eis::utils;

#define EIS_UDF_PROCESS "process"

// Cython lock, errors with the GIL occur if this does not exist
std::mutex g_cython_mtx;

PythonUdfHandle::PythonUdfHandle(std::string name, int max_workers) :
    UdfHandle(name, max_workers)
{
    m_udf_obj = NULL;
    m_udf_func = NULL;
}

PythonUdfHandle::~PythonUdfHandle() {
    LOG_DEBUG_0("Destroying Python UDF");

    LOG_DEBUG_0("Releasing process the function");
    if(m_udf_func != NULL && m_udf_func != Py_None)
        Py_DECREF(m_udf_func);

    LOG_DEBUG_0("Releasing process the Python object");
    if(m_udf_obj != NULL && m_udf_obj != Py_None)
        Py_DECREF(m_udf_obj);

    LOG_DEBUG_0("Finshed destroying the Python UDF");
}

bool PythonUdfHandle::initialize(config_t* config) {
    bool res = this->UdfHandle::initialize(config);
    if(!res)
        return false;

    LOG_DEBUG("Loading Python UDF: %s", get_name().c_str());
    if(PyArray_API == NULL) {
        import_array();
    }

    // Import Cython module
    LOG_DEBUG_0("Importing UDF library");
    PyObject* module = PyImport_ImportModule("udf");
    if(module == NULL) {
        LOG_ERROR_0("Failed to import udf Python module");
        PyErr_Print();
        return false;
    }

    // Load the Python UDF
    LOG_DEBUG_0("Loading the UDF");
    m_udf_obj = load_udf(get_name().c_str(), config);

    // Module no longer needed
    if(m_udf_obj == Py_None || PyErr_Occurred() != NULL) {
        LOG_ERROR_0("Failed to load UDF");
        if(PyErr_Occurred() != NULL) {
            PyErr_Print();
        }
        return false;
    }

    // Get the process() function from the Python object
    m_udf_func = PyObject_GetAttrString(m_udf_obj, EIS_UDF_PROCESS);
    if(m_udf_func == NULL) {
        LOG_ERROR_0("Failed to get process() method from UDF");
        if(PyErr_Occurred() != NULL) {
            PyErr_Print();
        }
        // No need to call Py_DECREF() on the object since it will be taken
        // care of in the destructor of this object.
        return false;
    }

    return true;
}

UdfRetCode PythonUdfHandle::process(Frame* frame) {
    // Get the lock to call cython
    std::lock_guard<std::mutex> lk(g_cython_mtx);

    // Create NumPy array shape
    std::vector<npy_intp> sizes;
    sizes.push_back(frame->get_height());
    sizes.push_back(frame->get_width());
    sizes.push_back(frame->get_channels());
    npy_intp* dims = sizes.data();

    // Create new NumPy Array
    PyObject* py_frame = PyArray_SimpleNewFromData(
            3, dims, NPY_UINT8, (void*) frame->get_data());

    // Call the UDF process method
    UdfRetCode ret = call_udf(m_udf_obj, py_frame, frame->get_meta_data());
    Py_DECREF(py_frame);
    if(PyErr_Occurred() != NULL) {
        LOG_ERROR_0("Error in UDF process() method");
        PyErr_Print();
        return UdfRetCode::UDF_ERROR;
    }

    return ret;
}
