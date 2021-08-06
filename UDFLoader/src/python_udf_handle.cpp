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
 */

// Defining NumPy version
#define NPY_NO_DEPRECATED_API NPY_1_14_API_VERSION

#include <vector>
#include <atomic>
#include <cstdlib>
#include <numpy/ndarrayobject.h>

#include <eii/utils/logger.h>
#include "eii/udf/python_udf_handle.h"
#include "cython/udf.h"

using namespace eii::udf;

#define EII_UDF_PROCESS "process"

PythonUdfHandle::PythonUdfHandle(std::string name, int max_workers) :
    UdfHandle(name, max_workers)
{
    m_udf_obj = NULL;
    m_udf_func = NULL;
}

PythonUdfHandle::~PythonUdfHandle() {
    LOG_DEBUG_0("Destroying Python UDF");

    LOG_DEBUG_0("Aquiring the GIL");
    PyGILState_STATE gstate;
    gstate = PyGILState_Ensure();
    LOG_DEBUG_0("Acquired GIL");

    LOG_DEBUG_0("Releasing process the function");
    if(m_udf_func != NULL && m_udf_func != Py_None)
        Py_DECREF(m_udf_func);

    LOG_DEBUG_0("Releasing process the Python object");
    if(m_udf_obj != NULL && m_udf_obj != Py_None)
        Py_DECREF(m_udf_obj);

    PyGILState_Release(gstate);
    LOG_DEBUG_0("Finshed destroying the Python UDF");
}

bool PythonUdfHandle::initialize(config_t* config) {
    bool res = this->UdfHandle::initialize(config);
    if(!res)
        return false;

    LOG_DEBUG("Has GIL: %d", PyGILState_Check());
    LOG_DEBUG_0("Aquiring the GIL");
    PyGILState_STATE gstate;
    gstate = PyGILState_Ensure();
    LOG_DEBUG_0("GIL acquired");

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
        PyGILState_Release(gstate);
        return false;
    }

    char* dev_mode = getenv("DEV_MODE");
    char* log_level = getenv("PY_LOG_LEVEL");
    cython_initialize(dev_mode, log_level);

    // Load the Python UDF
    LOG_DEBUG_0("Loading the UDF");
    m_udf_obj = load_udf(get_name().c_str(), config);
    LOG_DEBUG_0("UDF Loaded");

    // Module no longer needed
    if(m_udf_obj == Py_None || PyErr_Occurred() != NULL) {
        LOG_ERROR_0("Failed to load UDF");
        if(PyErr_Occurred() != NULL) {
            PyErr_Print();
        }
        PyGILState_Release(gstate);
        return false;
    }

    // Get the process() function from the Python object
    m_udf_func = PyObject_GetAttrString(m_udf_obj, EII_UDF_PROCESS);
    if(m_udf_func == NULL) {
        LOG_ERROR_0("Failed to get process() method from UDF");
        if(PyErr_Occurred() != NULL) {
            PyErr_Print();
        }
        PyGILState_Release(gstate);
        // No need to call Py_DECREF() on the object since it will be taken
        // care of in the destructor of this object.
        return false;
    }

    PyGILState_Release(gstate);

    return true;
}

void free_np_frame(void* varg) {
    LOG_DEBUG_0("Freeing Numpy array");
    LOG_DEBUG_0("Aquiring the GIL");
    PyGILState_STATE gstate;
    gstate = PyGILState_Ensure();
    LOG_DEBUG_0("Acquired GIL");

    PyObject* obj = (PyObject*) varg;
    Py_DECREF(obj);

    LOG_DEBUG_0("Releasing the GIL");
    PyGILState_Release(gstate);
    LOG_DEBUG_0("Released");
}

UdfRetCode PythonUdfHandle::process(Frame* frame) {
    // Get number of frames in Frame object
    int num_frames = frame->get_number_of_frames();
    PyObject* py_frame = Py_None;
    PyObject* output = Py_None;

    LOG_DEBUG_0("Aquiring the GIL");
    PyGILState_STATE gstate;
    gstate = PyGILState_Ensure();
    LOG_DEBUG_0("Acquired GIL");

    if (num_frames == 1) {
        std::vector<npy_intp> sizes;
        sizes.push_back(frame->get_height());
        sizes.push_back(frame->get_width());
        sizes.push_back(frame->get_channels());
        npy_intp* dims = sizes.data();

        // Create new NumPy Array
        py_frame = PyArray_SimpleNewFromData(
                3, dims, NPY_UINT8, (void*) frame->get_data(0));
    } else {
        PyObject* py_temp_frame = Py_None;
        py_frame = PyList_New(num_frames);
        for (int i = 0; i < num_frames; i++) {
            std::vector<npy_intp> sizes;
            sizes.push_back(frame->get_height(i));
            sizes.push_back(frame->get_width(i));
            sizes.push_back(frame->get_channels(i));
            npy_intp* dims = sizes.data();

            // Create new NumPy Array
            py_temp_frame = PyArray_SimpleNewFromData(
                    3, dims, NPY_UINT8, (void*) frame->get_data(i));

            // Append py_frame to py_list
            int result = PyList_SetItem(py_frame, i, py_temp_frame);
            if (result != 0) {
                throw "Failed to set py_frame in py_list";
            }
        }
    }

    LOG_DEBUG_0("Before process call");
    UdfRetCode ret = call_udf(
            m_udf_obj, py_frame, output, frame->get_meta_data());
    LOG_DEBUG_0("process call done");

    if(PyErr_Occurred() != NULL) {
        Py_DECREF(py_frame);
        LOG_ERROR_0("Error in UDF process() method");
        PyErr_Print();
        LOG_DEBUG_0("Releasing the GIL");
        PyGILState_Release(gstate);
        LOG_DEBUG_0("Released");
        return UdfRetCode::UDF_ERROR;
    }
    LOG_DEBUG_0("process done");

    // NOTE: If output == py_frame, then the UDF returned the same Python
    // object for the frame as was passed to it, this does not count as a
    // changed or updated frame.
    if(ret == UDF_FRAME_MODIFIED && output != py_frame) {
        LOG_DEBUG_0("Python modified frame");

        // If output is a list of numpy frames, call set_data for all
        // available numpy frames. Else set only for first frame
        if (!PyList_Check(output)) {
            PyArrayObject* py_array = (PyArrayObject*) output;

            int dims = PyArray_NDIM(py_array);
            if(dims < 3 || dims > 3) {
                LOG_ERROR(
                    "NumPy array has too many dimensions must be 3 not %d",
                    dims);
                Py_DECREF(output);
                Py_DECREF(py_frame);

                LOG_DEBUG_0("Releasing the GIL");
                PyGILState_Release(gstate);
                LOG_DEBUG_0("Released");

                return UdfRetCode::UDF_ERROR;
            }

            npy_intp* shape = PyArray_SHAPE(py_array);
            frame->set_data(
                    0, (void*) output, free_np_frame, PyArray_DATA(py_array),
                    shape[1], shape[0], shape[2]);

            ret = UDF_OK;
        } else {
            Py_ssize_t n = PyList_Size(output);
            for (int i = 0; i < n; i++) {
                PyObject* item = PyList_GetItem(output, i);

                PyArrayObject* py_array = (PyArrayObject*) item;
                int dims = PyArray_NDIM(py_array);
                if(dims < 3 || dims > 3) {
                    LOG_ERROR(
                        "NumPy array has too many dimensions must be 3 not %d",
                        dims);
                    Py_DECREF(output);
                    Py_DECREF(py_frame);

                    LOG_DEBUG_0("Releasing the GIL");
                    PyGILState_Release(gstate);
                    LOG_DEBUG_0("Released");

                    return UdfRetCode::UDF_ERROR;
                }

                npy_intp* shape = PyArray_SHAPE(py_array);

                frame->set_data(
                        i, (void*) item, free_np_frame, PyArray_DATA(py_array),
                        shape[1], shape[0], shape[2]);
            }
            ret = UDF_OK;
        }
    } else if (output == py_frame) {
        // If output == py_frame, then an extra DECREF is required to make sure
        // the Python NumPy array is released (this will not free the
        // underlying frame data).
        Py_DECREF(output);

        // The UDF can return OK in this instance
        ret = UDF_OK;
    }

    Py_DECREF(py_frame);

    LOG_DEBUG_0("Releasing the GIL");
    PyGILState_Release(gstate);
    LOG_DEBUG_0("Released");

    return ret;
}
