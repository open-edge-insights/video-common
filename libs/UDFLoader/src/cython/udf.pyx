# Copyright (c) 2019 Intel Corporation.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
# distutils: language = c++
"""UDF Cython code to interface with Python
"""
# Cython imports
from libc.stdint cimport *
from cpython cimport bool

# Python imports
import inspect
import importlib
import numpy as np


cdef extern from "stdbool.h":
    ctypedef bint c_bool

cdef extern from "eis/utils/config.h":
    ctypedef enum config_value_type_t:
        CVT_INTEGER  = 0
        CVT_FLOATING = 1
        CVT_STRING   = 2
        CVT_BOOLEAN  = 3
        CVT_OBJECT   = 4
        CVT_ARRAY    = 5
        CVT_NONE     = 6

    # Forward declaration
    ctypedef struct config_value_t

    ctypedef struct config_value_object_t:
        void* object
        config_value_t* (*get)(const void*,const char*)
        void (*free)(void* object)

    ctypedef struct config_value_array_t:
        void* array
        size_t length
        config_value_t* (*get)(void*,int)
        void (*free)(void*)

    ctypedef union config_value_type_body_union_t:
        int64_t      integer
        double       floating
        const char*  string
        c_bool       boolean
        config_value_object_t* object
        config_value_array_t* array

    ctypedef struct config_value_t:
        config_value_type_t type
        config_value_type_body_union_t body

    ctypedef config_value_t* (*get_config_value_fn)(const void*,const char*)

    ctypedef struct config_t:
        void* cfg
        void (*free)(void*)
        config_value_t* (*get_config_value)(const void*, const char*)

    config_value_t* config_get(const config_t* config, const char* key)
    void config_value_destroy(config_value_t* config_value)
    config_value_t* config_value_object_get(
        const config_value_t* obj, const char* key);
    config_value_t* config_value_array_get(const config_value_t* arr, int idx);
    size_t config_value_array_len(const config_value_t* arr);


cdef extern from "eis/msgbus/msg_envelope.h":
    ctypedef enum msgbus_ret_t:
        MSG_SUCCESS = 0
        MSG_ERR_PUB_FAILED = 1
        MSG_ERR_SUB_FAILED = 2
        MSG_ERR_RESP_FAILED = 3
        MSG_ERR_RECV_FAILED = 4
        MSG_ERR_RECV_EMPTY = 5
        MSG_ERR_ALREADY_RECEIVED = 6
        MSG_ERR_NO_SUCH_SERVICE = 7
        MSG_ERR_SERVICE_ALREADY_EXIST = 8,
        MSG_ERR_BUS_CONTEXT_DESTROYED = 9
        MSG_ERR_INIT_FAILED = 10
        MSG_ERR_NO_MEMORY = 11
        MSG_ERR_ELEM_NOT_EXIST = 12
        MSG_ERR_ELEM_ALREADY_EXISTS = 13
        MSG_ERR_ELEM_BLOB_ALREADY_SET = 14
        MSG_ERR_ELEM_BLOB_MALFORMED = 15
        MSG_RECV_NO_MESSAGE = 16
        MSG_ERR_SERVICE_INIT_FAILED = 17
        MSG_ERR_REQ_FAILED = 18
        MSG_ERR_EINTR = 19
        MSG_ERR_MSG_SEND_FAILED = 20
        MSG_ERR_DISCONNECTED = 21
        MSG_ERR_AUTH_FAILED = 22
        MSG_ERR_UNKNOWN = 255

    ctypedef struct owned_blob_t:
        void* ptr
        void (*free)(void*)
        c_bool owned
        size_t len
        const char* bytes

    ctypedef enum content_type_t:
        CT_JSON = 0
        CT_BLOB = 1

    ctypedef enum msg_envelope_data_type_t:
        MSG_ENV_DT_INT      = 0
        MSG_ENV_DT_FLOATING = 1
        MSG_ENV_DT_STRING   = 2
        MSG_ENV_DT_BOOLEAN  = 3
        MSG_ENV_DT_BLOB     = 4

    ctypedef struct msg_envelope_blob_t:
        owned_blob_t* shared
        uint64_t len
        const char*    data

    ctypedef union msg_envelope_elem_body_body_t:
        int64_t integer
        double floating
        char* string
        c_bool boolean
        msg_envelope_blob_t* blob

    ctypedef struct msg_envelope_elem_body_t:
        msg_envelope_data_type_t type
        msg_envelope_elem_body_body_t body

    ctypedef struct msg_envelope_elem_t:
        char* key
        c_bool in_use
        msg_envelope_elem_body_t* body

    ctypedef struct msg_envelope_t:
        char* correlation_id
        content_type_t content_type
        int size
        int max_size
        msg_envelope_elem_t* elems
        msg_envelope_elem_body_t* blob

    msg_envelope_elem_body_t* msgbus_msg_envelope_new_string(
            const char* string)
    msg_envelope_elem_body_t* msgbus_msg_envelope_new_integer(int64_t integer)
    msg_envelope_elem_body_t* msgbus_msg_envelope_new_floating(double floating)
    msg_envelope_elem_body_t* msgbus_msg_envelope_new_bool(c_bool boolean)
    msg_envelope_elem_body_t* msgbus_msg_envelope_new_blob(
            const char* data, size_t len)
    void msgbus_msg_envelope_elem_destroy(msg_envelope_elem_body_t* elem)
    msgbus_ret_t msgbus_msg_envelope_put(
            msg_envelope_t* env, const char* key,
            msg_envelope_elem_body_t* data)


cdef extern from "eis/udf/udfretcodes.h" namespace "eis::udf":
    ctypedef enum UdfRetCode:
        UDF_OK = 0
        UDF_DROP_FRAME = 1
        UDF_ERROR = 255


cdef class ConfigurationObject:
    """Wrapper object around a config_value_t structure which is a CVT_OBJECT
    type. This object provides the interfaces for retrieving values from the
    object as if it were a Python dictionary.
    """
    cdef config_value_t* _value

    def __cinit__(self):
        """Cython constructor
        """
        self._value = NULL

    def __dealloc__(self):
        """Cython destructor
        """
        # NOTE: Not freeing _config because that should be freed by the
        # Configuration object
        if self._value != NULL:
            config_value_destroy(self._value)

    def __getitem__(self, key):
        """Get item from the configuration object

        :param key: Key for the value in the object
        :type: str
        :return: Python object for the value
        :rtype: object
        """
        cdef config_value_t* value
        bkey = bytes(key, 'utf-8')
        value = config_value_object_get(self._value, bkey)
        if value == NULL:
            raise KeyError(key)
        return cfv_to_object(value)

    @staticmethod
    cdef create(config_value_t* value):
        """Helper method to initialize a configuration object.
        """
        assert value.type == CVT_OBJECT, 'Config value must be an object'
        c = ConfigurationObject()
        c._value = value
        return c


cdef object cfv_to_object(config_value_t* value):
    """Convert a config_value_t* to a Python object.

    :param cv: Configuration value to convert
    :type: config_value_t*
    :return: Python object
    :type: object
    """
    cdef config_value_t* arr_val
    ret_val = None

    if value.type == CVT_INTEGER:
        ret_val = <int> value.body.integer
    elif value.type == CVT_FLOATING:
        ret_val = <float> value.body.floating
    elif value.type == CVT_STRING:
        ret_val = <bytes> value.body.string
        ret_val = ret_val.decode('utf-8')
    elif value.type == CVT_BOOLEAN:
        ret_val = <bool> value.body.boolean
    elif value.type == CVT_NONE:
        pass  # Do nothing here, this should return None
    elif value.type == CVT_OBJECT:
        # This is a special case for returning because we do not want the
        # config_value_t structure to be destroyed at the end
        return ConfigurationObject.create(value)
        # return None
    elif value.type == CVT_ARRAY:
        # Recursively convert all array values
        arr = []
        ret_val = []
        length = config_value_array_len(value)
        for i in range(length):
            arr_val = config_value_array_get(value, i)
            if arr_val == NULL:
                raise IndexError(f'Cannot get element: {i}')
            py_value = cfv_to_object(arr_val)
            # TODO: Veirfy that arr_val is not leaked...
            arr.append(py_value)
        ret_val = arr
    else:
        config_value_destroy(value)
        raise RuntimeError('Unknown CVT type, this should never happen')

    config_value_destroy(value)
    return ret_val


cdef public object load_udf(const char* name, config_t* config):
    """Load Python UDF.

    :param name: Name of the UDF to load (can be full package path)
    :type: const char*
    :param config: Configuration for the UDF
    :type: config_t*
    :return: Python UDF object
    :type: object
    """
    cdef config_value_t* value

    py_name = <bytes> name
    py_name = py_name.decode('utf-8')

    try:
        lib = importlib.import_module(f'{py_name}')

        arg_names = inspect.getargspec(lib.Udf.__init__).args[1:]
        if len(arg_names) > 0:
            # Skipping the first argument since it is the self argument
            args = []
            for a in arg_names:
                key = bytes(a, 'utf-8')
                value = config_get(config, key)
                if value == NULL:
                    raise KeyError(f'UDF config missing key: {a}')
                py_value = cfv_to_object(value)
                args.append(py_value)
        else:
            args = []

        return lib.Udf(*args)
    except AttributeError:
        raise AttributeError(f'{py_name} module is missing the Udf class')
    except ImportError:
        raise ImportError(f'Failed to load UDF: {py_name}')


cdef public UdfRetCode call_udf(
        object udf, object frame, msg_envelope_t* meta) except *:
    """Call UDF
    """
    cdef msgbus_ret_t ret
    cdef msg_envelope_elem_body_t* body
    cdef content_type_t ct
    cdef char* key = NULL

    drop, new_meta = udf.process(frame)

    if drop:
        return UDF_DROP_FRAME

    if new_meta is not None:
        for k,v in new_meta.items():
            if isinstance(v, str):
                bv = bytes(v, 'utf-8')
                body = msgbus_msg_envelope_new_string(bv)
            elif isinstance(v, int):
                body = msgbus_msg_envelope_new_integer(<int64_t> v)
            elif isinstance(v, float):
                body = msgbus_msg_envelope_new_floating(<double> v)
            elif isinstance(v, bool):
                body = msgbus_msg_envelope_new_bool(<bint> v)
            else:
                raise ValueError(f'Unknown data type in dict: {type(v)}')

            k = bytes(k, 'utf-8')
            ret = msgbus_msg_envelope_put(meta, <char*> k, body)
            if ret != msgbus_ret_t.MSG_SUCCESS:
                msgbus_msg_envelope_elem_destroy(body)
                raise RuntimeError(f'Failed to put element {k}')
            else:
                # The message envelope takes ownership of the memory allocated
                # for these elements. Setting to NULL to keep the state clean.
                body = NULL
                key = NULL

    return UDF_OK
