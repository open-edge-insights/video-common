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
 * @brief Implementation of @c Frame class
 */

#include <vector>
#include <opencv2/opencv.hpp>
#include <safe_lib.h>
#include <eii/utils/logger.h>

#include "eii/udf/frame.h"

using namespace eii::udf;

// Prototyes
void free_decoded(void* varg);
void free_encoded(void* varg);

/**
 * Helper method to verify the correct encoding level is set for the given
 * encoding type.
 */
bool verify_encoding_level(EncodeType encode_type, int encode_level);


Frame::Frame(void* frame, int width, int height, int channels, void** data,
             void (*free_frame)(void*), int num_frames, EncodeType encode_type,
             int encode_level) :
    Serializable(NULL), m_frame(frame), m_free_frame(free_frame),
    m_data(data), m_blob_ptr(NULL), m_meta_data(NULL), m_width(width),
    m_height(height), m_channels(channels),
    m_num_frames(num_frames), m_serialized(false),
    m_encode_type(encode_type), m_encode_level(encode_level)
{
    if(m_free_frame == NULL) {
        throw "The free_frame() method cannot be NULL";
    }
    if(!verify_encoding_level(encode_type, encode_level)) {
        throw "Encode level invalid for the encoding type";
    }

    msgbus_ret_t ret = MSG_SUCCESS;
    m_meta_data = msgbus_msg_envelope_new(CT_JSON);
    if(m_meta_data == NULL) {
        throw "Failed to initialize meta data envelope";
    }

    // Add frame details to meta data
    msg_envelope_elem_body_t* e_width = msgbus_msg_envelope_new_integer(
            width);
    if(e_width == NULL) {
        throw "Failed to initialize width meta-data";
    }
    ret = msgbus_msg_envelope_put(m_meta_data, "width", e_width);
    if(ret != MSG_SUCCESS) {
        throw "Failed to put width meta-data";
    }

    // Add height
    msg_envelope_elem_body_t* e_height = msgbus_msg_envelope_new_integer(
            height);
    if(e_height == NULL) {
        throw "Failed to initialize height meta-data";
    }

    ret = msgbus_msg_envelope_put(m_meta_data, "height", e_height);
    if(ret != MSG_SUCCESS) {
        msgbus_msg_envelope_elem_destroy(e_width);
        throw "Failed to put height meta-data";
    }

    // Add channels
    msg_envelope_elem_body_t* e_channels = msgbus_msg_envelope_new_integer(
            channels);
    if(e_channels == NULL) {
        throw "Failed to initialize channels meta-data";
    }
    ret = msgbus_msg_envelope_put(m_meta_data, "channels", e_channels);
    if(ret != MSG_SUCCESS) {
        msgbus_msg_envelope_elem_destroy(e_height);
        msgbus_msg_envelope_elem_destroy(e_width);
        throw "Failed to put channels meta-data";
    }

    // Add encoding (if type is not NONE)
    if(encode_type != EncodeType::NONE) {
        msg_envelope_elem_body_t* e_enc_type = NULL;
        if(encode_type == EncodeType::JPEG) {
            e_enc_type = msgbus_msg_envelope_new_string("jpeg");
        } else {
            e_enc_type = msgbus_msg_envelope_new_string("png");
        }
        if(e_enc_type == NULL) {
            throw "Failed initialize encoding type meta-data";
        }

        msg_envelope_elem_body_t* e_enc_lvl = msgbus_msg_envelope_new_integer(
                encode_level);
        if(e_enc_lvl == NULL) {
            throw "Failed to initialize encoding level meta-data";
        }

        ret = msgbus_msg_envelope_put(m_meta_data, "encoding_type", e_enc_type);
        if(ret != MSG_SUCCESS) {
            throw "Failed to put encoding type in object";
        }

        ret = msgbus_msg_envelope_put(m_meta_data, "encoding_level", e_enc_lvl);
        if(ret != MSG_SUCCESS) {
            throw "Failed to put encoding level in object";
        }
    }
}

Frame::Frame(msg_envelope_t* msg) :
    Serializable(msg), m_frame(NULL), m_free_frame(NULL),
    m_blob_ptr(NULL), m_meta_data(msg), m_serialized(false)
{
    msgbus_ret_t ret = MSG_SUCCESS;

    // Get frame data
    msg_envelope_elem_body_t* blob = NULL;
    ret = msgbus_msg_envelope_get(msg, NULL, &blob);
    if(ret != MSG_SUCCESS) {
        throw "Failed to retrieve frame blob from  msg envelope";
    } else if (blob->type == MSG_ENV_DT_BLOB) {
        m_num_frames = 1;
        m_data = (void**) malloc(sizeof(void*) * m_num_frames);
        if (m_data == NULL) {
            throw "Malloc failed for m_data";
        }
        m_data[0] = (void*) blob->body.blob->data;
    } else if (blob->type == MSG_ENV_DT_ARRAY) {
        m_num_frames = blob->body.array->len;
        m_data = (void**) malloc(sizeof(void*) * m_num_frames);
        if (m_data == NULL) {
            throw "Malloc failed for m_data";
        }
        msg_envelope_elem_body_t* env_get_data;
        for (int i = 0; i < m_num_frames; i++) {
            env_get_data = msgbus_msg_envelope_elem_array_get_at(blob, i);
            m_data[i] = (void*) env_get_data->body.blob->data;
        }
        env_get_data = NULL;
    }

    // Get frame width
    msg_envelope_elem_body_t* width = NULL;
    ret = msgbus_msg_envelope_get(msg, "width", &width);
    if(ret != MSG_SUCCESS) {
        throw "Failed to retrieve width";
    } else if(width->type != MSG_ENV_DT_INT) {
        throw "Frame width must be an integer";
    }
    m_width = width->body.integer;

    // Get frame height
    msg_envelope_elem_body_t* height = NULL;
    ret = msgbus_msg_envelope_get(msg, "height", &height);
    if(ret != MSG_SUCCESS) {
        throw "Failed to retrieve height";
    } else if(height->type != MSG_ENV_DT_INT) {
        throw "Frame height must be an integer";
    }
    m_height = height->body.integer;

    // Get frame channels
    msg_envelope_elem_body_t* channels = NULL;
    ret = msgbus_msg_envelope_get(msg, "channels", &channels);
    if(ret != MSG_SUCCESS) {
        throw "Failed to retrieve channels";
    } else if(channels->type != MSG_ENV_DT_INT) {
        throw "Frame channels must be an integer";
    }
    m_channels = channels->body.integer;

    // Get encoding from configuation
    msg_envelope_elem_body_t* enc_type = NULL;
    ret = msgbus_msg_envelope_get(msg, "encoding_type", &enc_type);
    if(ret == MSG_SUCCESS) {
        LOG_DEBUG_0("Frame is encoded");
        if(enc_type->type != MSG_ENV_DT_STRING) {
            throw "Encoding type must be a string";
        }

        msg_envelope_elem_body_t* enc_lvl = NULL;
        ret = msgbus_msg_envelope_get(msg, "encoding_level", &enc_lvl);
        if(enc_lvl == NULL) {
            throw "Encoding level missing from frame meta-data";
        } else if(enc_lvl->type != MSG_ENV_DT_INT) {
            throw "Encoding level must be an integer";
        }

        int ind_jpeg = 0;
        strcmp_s(enc_type->body.string, strlen(enc_type->body.string),
                "jpeg", &ind_jpeg);

        int ind_png = 0;
        strcmp_s(enc_type->body.string, strlen(enc_type->body.string),
                 "png", &ind_png);

        if(ind_jpeg == 0 || ind_png == 0) {
            m_encode_level = enc_lvl->body.integer;

            if(ind_jpeg == 0) {
                LOG_DEBUG_0("Frame encoded as a JPEG");
                m_encode_type = EncodeType::JPEG;
            } else if(ind_png == 0) {
                LOG_DEBUG_0("Frame encoded as a PNG");
                m_encode_type = EncodeType::PNG;
            }

            // Construct vector of bytes to pass to cv::imdecode
            std::vector<uchar> data;
            uchar* buf = NULL;
            size_t len = 0;
            if (blob->type == MSG_ENV_DT_ARRAY) {
                msg_envelope_elem_body_t* env_get_data = msgbus_msg_envelope_elem_array_get_at(blob, 0);
                buf = (uchar*) env_get_data->body.blob->data;
                len = (size_t) env_get_data->body.blob->len;
            }
            if (blob->type == MSG_ENV_DT_BLOB) {
                buf = (uchar*) blob->body.blob->data;
                len = (size_t) blob->body.blob->len;
            }
            data.assign(buf, buf + len);
            // Decode the frame
            // TODO: Should we always decode as color?
            cv::Mat* decoded = new cv::Mat();
            *decoded = cv::imdecode(data, cv::IMREAD_COLOR);
            if(decoded->empty()) {
                delete decoded;
                throw "Failed to decode the encoded frame";
            }
            data.clear();
            this->set_data((void*) decoded, decoded->cols, decoded->rows,
                           decoded->channels(), decoded->data, free_decoded, 0);
        } else {
            throw "Unknown encoding type";
        }
    } else {
        LOG_DEBUG_0("Frame is not encoded");
        m_encode_type = EncodeType::NONE;
        m_encode_level = 0;
    }
}

Frame::Frame(const Frame& src) {
    // This method does nothing, because the object is not supposed to be
    // copied
}

Frame::~Frame() {
    if(!m_serialized.load()) {
        // Free the underlying frame if the m_free_frame method is set
        if(m_free_frame != NULL) {
            this->m_free_frame(this->m_frame);
        }

        // Destroy the meta data message envelope if the frame is not from a
        // deserialized message and if the frame itself has not been serialized
        if(m_msg == NULL && !m_serialized.load()) {
            msgbus_msg_envelope_destroy(m_meta_data);
        }
    }

    // else: If m_msg is not NULL, then the Frame was deserialized from a
    // msg_envelope_t received over the msgbus. If this is the case, then
    // the Deserialized() destructure will delete the m_msg envelope struct
    // cleaning up all data used by this object.
}


// local helper function to fetch any multi frame parameters
msg_envelope_elem_body_t* fetch_multi_frame_parameter(msg_envelope_t* meta_data, int index, char* key) {
    msg_envelope_elem_body_t* additional_frames_arr;
    msgbus_ret_t ret = msgbus_msg_envelope_get(meta_data, "additional_frames", &additional_frames_arr);
    if (ret != MSG_SUCCESS) {
        LOG_INFO_0("Failed to fetch additional_frames object");
        throw "Failed to fetch additional_frames object";
    }
    if (additional_frames_arr->type != MSG_ENV_DT_ARRAY) {
        LOG_INFO_0("additional_frames type not an array");
        throw "additional_frames type not an array";
    }
    msg_envelope_elem_body_t* obj = msgbus_msg_envelope_elem_array_get_at(additional_frames_arr, (index - 1));
    if (obj == NULL) {
        LOG_INFO_0("Unable to fetch meta-data");
        throw "Unable to fetch meta-data";
    }
    msg_envelope_elem_body_t* key_obj = msgbus_msg_envelope_elem_object_get(obj, key);
    if (key_obj == NULL) {
        LOG_INFO_0("Unable to fetch requested element from metadata");
        throw "Unable to fetch requested element from metadata";
    }
    return key_obj;
}

int Frame::get_width(int index) {
    if (index == 0)
        return m_width;
    msg_envelope_elem_body_t* width = fetch_multi_frame_parameter(m_meta_data, index, "width");
    if (width == NULL) {
        LOG_INFO_0("Unable to fetch width in additional frames array");
        throw "Unable to fetch width in additional frames array";
    }
    if (width->type != MSG_ENV_DT_INT) {
        LOG_INFO_0("width not an integer");
        throw "width not an integer";
    }
    return width->body.integer;
}

int Frame::get_height(int index) {
    if (index == 0)
        return m_height;
    msg_envelope_elem_body_t* height = fetch_multi_frame_parameter(m_meta_data, index, "height");
    if (height == NULL) {
        throw "Unable to fetch height in additional frames array";
    }
    if (height->type != MSG_ENV_DT_INT) {
        throw "height not an integer";
    }
    return height->body.integer;
}

int Frame::get_channels(int index) {
    if (index == 0)
        return m_channels;
    msg_envelope_elem_body_t* channels = fetch_multi_frame_parameter(m_meta_data, index, "channels");
    if (channels == NULL) {
        throw "Unable to fetch channels in additional frames array";
    }
    if (channels->type != MSG_ENV_DT_INT) {
        throw "channels not an integer";
    }
    return channels->body.integer;
}

EncodeType Frame::get_encode_type(int index) {
    if (index == 0)
        return m_encode_type;
    msg_envelope_elem_body_t* encoding_type = fetch_multi_frame_parameter(m_meta_data, index, "encoding_type");
    if (encoding_type == NULL) {
        throw "Unable to fetch encoding_type in additional frames array";
    }
    if (encoding_type->type != MSG_ENV_DT_STRING) {
        throw "encoding_type not a string";
    }
    char* enc_type = encoding_type->body.string;
    EncodeType encd_type = EncodeType::NONE;
    if (strcmp(enc_type, "jpeg") == 0) {
        encd_type = EncodeType::JPEG;
        LOG_DEBUG_0("Encoding type is jpeg");
    } else if (strcmp(enc_type, "png") == 0) {
        encd_type = EncodeType::PNG;
        LOG_DEBUG_0("Encoding type is png");
    } else if (strcmp(enc_type, "none") == 0) {
        LOG_DEBUG_0("Encoding disabled");
    } else {
        throw "Encoding type is not supported";
    }
    return encd_type;
}

int Frame::get_encode_level(int index) {
    if (index == 0)
        return m_encode_level;
    msg_envelope_elem_body_t* encoding_level = fetch_multi_frame_parameter(m_meta_data, index, "encoding_level");
    if (encoding_level == NULL) {
        throw "Unable to fetch encoding_level in additional frames array";
    }
    if (encoding_level->type != MSG_ENV_DT_INT) {
        throw "encoding_level not an integer";
    }
    return encoding_level->body.integer;
}

void* Frame::get_data(int index) {
    if(m_serialized.load()) {
        LOG_ERROR_0(
                "Writable data method called after frame serialization");
        return NULL;
    }
    return m_data[index];
}

int Frame::get_number_of_frames() {
    return m_num_frames;
}

bool Frame::set_multi_frame_parameters(int index, int width,
                                       int height, int channels,
                                       char* encoding_type,
                                       int encoding_level) {
    if (index == 0) {
        this->m_width = width;
        this->m_height = height;
        this->m_channels = channels;
        this->m_encode_level = encoding_level;
        if (strcmp(encoding_type, "jpeg") == 0) {
            m_encode_type = EncodeType::JPEG;
            LOG_DEBUG_0("Encoding type is jpeg");
        } else if (strcmp(encoding_type, "png") == 0) {
            m_encode_type = EncodeType::PNG;
            LOG_DEBUG_0("Encoding type is png");
        } else {
            throw "Encoding type is not supported";
        }
        return true;
    }
    msg_envelope_t* meta_data = this->get_meta_data();
    if (meta_data == NULL) {
        throw "Unable to fetch meta-data";
    }
    msgbus_ret_t ret = MSG_SUCCESS;
    msg_envelope_elem_body_t* additional_frames = \
        msgbus_msg_envelope_new_array();
    if (additional_frames == NULL) {
        msgbus_msg_envelope_elem_destroy(additional_frames);
        throw "Unable to fetch meta-data";
    }

    msg_envelope_elem_body_t* obj = msgbus_msg_envelope_new_object();
    if (obj == NULL) {
        msgbus_msg_envelope_elem_destroy(additional_frames);
        msgbus_msg_envelope_elem_destroy(obj);
        throw "Unable to fetch meta-data";
    }
    msg_envelope_elem_body_t* e_width = msgbus_msg_envelope_new_integer(width);
    if (e_width == NULL) {
        msgbus_msg_envelope_elem_destroy(additional_frames);
        msgbus_msg_envelope_elem_destroy(obj);
        msgbus_msg_envelope_elem_destroy(e_width);
        throw "Unable to fetch meta-data";
    }
    ret = msgbus_msg_envelope_elem_object_put(obj, "width", e_width);
    if (ret != MSG_SUCCESS) {
        msgbus_msg_envelope_elem_destroy(additional_frames);
        msgbus_msg_envelope_elem_destroy(obj);
        msgbus_msg_envelope_elem_destroy(e_width);
        throw "Unable to put width into meta-data";
    }
    msg_envelope_elem_body_t* e_height = \
        msgbus_msg_envelope_new_integer(height);
    if (e_height == NULL) {
        msgbus_msg_envelope_elem_destroy(additional_frames);
        msgbus_msg_envelope_elem_destroy(obj);
        msgbus_msg_envelope_elem_destroy(e_width);
        msgbus_msg_envelope_elem_destroy(e_height);
        throw "Unable to fetch meta-data";
    }
    ret = msgbus_msg_envelope_elem_object_put(obj, "height", e_height);
    if (ret != MSG_SUCCESS) {
        msgbus_msg_envelope_elem_destroy(additional_frames);
        msgbus_msg_envelope_elem_destroy(obj);
        msgbus_msg_envelope_elem_destroy(e_width);
        msgbus_msg_envelope_elem_destroy(e_height);
        throw "Unable to put height into meta-data";
    }
    msg_envelope_elem_body_t* e_channels = \
        msgbus_msg_envelope_new_integer(channels);
    if (e_channels == NULL) {
        msgbus_msg_envelope_elem_destroy(additional_frames);
        msgbus_msg_envelope_elem_destroy(obj);
        msgbus_msg_envelope_elem_destroy(e_width);
        msgbus_msg_envelope_elem_destroy(e_height);
        msgbus_msg_envelope_elem_destroy(e_channels);
        throw "Unable to fetch meta-data";
    }
    ret = msgbus_msg_envelope_elem_object_put(obj, "channels", e_channels);
    if (ret != MSG_SUCCESS) {
        msgbus_msg_envelope_elem_destroy(additional_frames);
        msgbus_msg_envelope_elem_destroy(obj);
        msgbus_msg_envelope_elem_destroy(e_width);
        msgbus_msg_envelope_elem_destroy(e_height);
        msgbus_msg_envelope_elem_destroy(e_channels);
        throw "Unable to put channels into meta-data";
    }
    msg_envelope_elem_body_t* e_encoding_type = \
        msgbus_msg_envelope_new_string(encoding_type);
    if (e_encoding_type == NULL) {
        msgbus_msg_envelope_elem_destroy(additional_frames);
        msgbus_msg_envelope_elem_destroy(obj);
        msgbus_msg_envelope_elem_destroy(e_width);
        msgbus_msg_envelope_elem_destroy(e_height);
        msgbus_msg_envelope_elem_destroy(e_channels);
        msgbus_msg_envelope_elem_destroy(e_encoding_type);
        throw "Unable to fetch meta-data";
    }
    ret = msgbus_msg_envelope_elem_object_put(obj, "encoding_type",
                                              e_encoding_type);
    if (ret != MSG_SUCCESS) {
        msgbus_msg_envelope_elem_destroy(additional_frames);
        msgbus_msg_envelope_elem_destroy(obj);
        msgbus_msg_envelope_elem_destroy(e_width);
        msgbus_msg_envelope_elem_destroy(e_height);
        msgbus_msg_envelope_elem_destroy(e_channels);
        msgbus_msg_envelope_elem_destroy(e_encoding_type);
        throw "Unable to put channels into meta-data";
    }
    msg_envelope_elem_body_t* e_encoding_level = \
        msgbus_msg_envelope_new_integer(encoding_level);
    if (e_encoding_level == NULL) {
        msgbus_msg_envelope_elem_destroy(additional_frames);
        msgbus_msg_envelope_elem_destroy(obj);
        msgbus_msg_envelope_elem_destroy(e_width);
        msgbus_msg_envelope_elem_destroy(e_height);
        msgbus_msg_envelope_elem_destroy(e_channels);
        msgbus_msg_envelope_elem_destroy(e_encoding_type);
        msgbus_msg_envelope_elem_destroy(e_encoding_level);
        throw "Unable to fetch meta-data";
    }
    ret = msgbus_msg_envelope_elem_object_put(obj, "encoding_level",
                                              e_encoding_level);
    if (ret != MSG_SUCCESS) {
        msgbus_msg_envelope_elem_destroy(additional_frames);
        msgbus_msg_envelope_elem_destroy(obj);
        msgbus_msg_envelope_elem_destroy(e_width);
        msgbus_msg_envelope_elem_destroy(e_height);
        msgbus_msg_envelope_elem_destroy(e_channels);
        msgbus_msg_envelope_elem_destroy(e_encoding_type);
        msgbus_msg_envelope_elem_destroy(e_encoding_level);
        throw "Unable to put channels into meta-data";
    }

    ret = msgbus_msg_envelope_elem_array_add(additional_frames, obj);
    if (ret != MSG_SUCCESS) {
        msgbus_msg_envelope_elem_destroy(additional_frames);
        msgbus_msg_envelope_elem_destroy(obj);
        msgbus_msg_envelope_elem_destroy(e_width);
        msgbus_msg_envelope_elem_destroy(e_height);
        msgbus_msg_envelope_elem_destroy(e_channels);
        msgbus_msg_envelope_elem_destroy(e_encoding_type);
        msgbus_msg_envelope_elem_destroy(e_encoding_level);
        throw "Unable to put channels into meta-data";
    }

    ret = msgbus_msg_envelope_put(meta_data, "additional_frames",
                                  additional_frames);
    if (ret != MSG_SUCCESS) {
        msgbus_msg_envelope_elem_destroy(additional_frames);
        msgbus_msg_envelope_elem_destroy(obj);
        msgbus_msg_envelope_elem_destroy(e_width);
        msgbus_msg_envelope_elem_destroy(e_height);
        msgbus_msg_envelope_elem_destroy(e_channels);
        msgbus_msg_envelope_elem_destroy(e_encoding_type);
        msgbus_msg_envelope_elem_destroy(e_encoding_level);
        throw "Unable to put channels into meta-data";
    }
    return true;
}

void Frame::set_data(
        void* frame, int width, int height, int channels, void* data,
        void (*free_frame)(void*), int index)
{
    msgbus_ret_t ret = MSG_SUCCESS;

    if(m_serialized.load()) {
        LOG_ERROR_0("Cannot set data after serialization");
        throw "Cannot set data after serialization";
    }

    // Release old data
    if(m_msg == NULL) {
        LOG_DEBUG("Setting new free_Frame");
        // Initialized from the first constructor
        this->m_free_frame(this->m_frame);
        this->m_free_frame = free_frame;
    } else {
        // This is a deserialized frame
        // NOTE: This doing major surgery on the underlying data in the
        // message envlope, this is not generally recommend and should
        // probably be made better in the future

        int len = width * height * channels;
        if (index == 0) {
            if (m_encode_type != EncodeType::NONE) {
                len = ((std::vector<uchar>*) frame)->size();
            }
        }

        if (m_msg->blob->type == MSG_ENV_DT_BLOB) {

            // Create the new blob element
            msg_envelope_elem_body_t* blob = msgbus_msg_envelope_new_blob(
                    (char*) data, len);
            if (blob == NULL) {
                throw "Failed to initialize new blob in Frame::set_data()";
            }

            // Destroy the old blob
            msgbus_msg_envelope_elem_destroy(m_msg->blob);

            // Set old blob to NULL so that it can be set again
            m_msg->blob = NULL;

            // Set correct ownership/blob pointers for memory management
            blob->body.blob->shared->ptr = frame;
            blob->body.blob->shared->free = free_frame;
            blob->body.blob->shared->owned = true;

            // Re-add the blob to the message envelope
            ret = msgbus_msg_envelope_put(m_msg, NULL, blob);
            if (ret != MSG_SUCCESS) {
                throw "Failed to re-add new blob data to received msg envelope";
            }
        } else if (m_msg->blob->type == MSG_ENV_DT_ARRAY) {
            // TODO
            // Add a put_at method for linked_list and replace this
            // way of resetting all blobs with that API

            // Create the new blob element
            msg_envelope_elem_body_t* blob = msgbus_msg_envelope_new_blob(
                    (char*) data, len);
            if (blob == NULL) {
                throw "Failed to initialize new blob in Frame::set_data()";
            }

            if (index == m_num_frames - 1) {
                blob->body.blob->shared->ptr = frame;
                blob->body.blob->shared->free = free_frame;
            } else {
                blob->body.blob->shared->ptr = frame;
                blob->body.blob->shared->free = NULL;
            }

            // Remove existing blob at index and re-add data to the end
            ret = msgbus_msg_envelope_elem_array_remove_at(m_msg->blob, index);
            if (ret != MSG_SUCCESS) {
                throw "Failed to remove existing blob data from msg envelope";
            }

            ret = msgbus_msg_envelope_put(m_msg, NULL, blob);
            if (ret != MSG_SUCCESS) {
                throw "Failed to re-add new blob data to received msg envelope";
            }
        }
    }

    // Repoint all internal data structures
    this->m_frame = frame;
    this->m_data[index] = data;

    // Add width
    msg_envelope_elem_body_t* e_width = msgbus_msg_envelope_new_integer(
            width);
    if (e_width == NULL) {
        msgbus_msg_envelope_elem_destroy(e_width);
        throw "Failed to initialize width meta-data";
    }

    // Add height
    msg_envelope_elem_body_t* e_height = msgbus_msg_envelope_new_integer(
            height);
    if (e_height == NULL) {
        msgbus_msg_envelope_elem_destroy(e_width);
        msgbus_msg_envelope_elem_destroy(e_height);
        throw "Failed to initialize height meta-data";
    }

    // Add channels
    msg_envelope_elem_body_t* e_channels = msgbus_msg_envelope_new_integer(
            channels);
    if (e_channels == NULL) {
        msgbus_msg_envelope_elem_destroy(e_width);
        msgbus_msg_envelope_elem_destroy(e_height);
        msgbus_msg_envelope_elem_destroy(e_channels);
        throw "Failed to initialize channels meta-data";
    }

    if (index == 0) {

        this->m_width = width;
        this->m_height = height;
        this->m_channels = channels;

        // Update meta-data for width, height, and channels
        // Remove old meta-data
        msgbus_msg_envelope_remove(m_meta_data, "width");
        msgbus_msg_envelope_remove(m_meta_data, "height");
        msgbus_msg_envelope_remove(m_meta_data, "channels");

        ret = msgbus_msg_envelope_put(m_meta_data, "width", e_width);
        if (ret != MSG_SUCCESS) {
            msgbus_msg_envelope_elem_destroy(e_width);
            msgbus_msg_envelope_elem_destroy(e_height);
            msgbus_msg_envelope_elem_destroy(e_channels);
            throw "Failed to put width meta-data";
        }

        ret = msgbus_msg_envelope_put(m_meta_data, "height", e_height);
        if (ret != MSG_SUCCESS) {
            msgbus_msg_envelope_elem_destroy(e_width);
            msgbus_msg_envelope_elem_destroy(e_height);
            msgbus_msg_envelope_elem_destroy(e_channels);
            throw "Failed to put height meta-data";
        }

        ret = msgbus_msg_envelope_put(m_meta_data, "channels", e_channels);
        if (ret != MSG_SUCCESS) {
            msgbus_msg_envelope_elem_destroy(e_width);
            msgbus_msg_envelope_elem_destroy(e_height);
            msgbus_msg_envelope_elem_destroy(e_channels);
            throw "Failed to put channels meta-data";
        }
    } else {
        msg_envelope_elem_body_t* additional_frames_arr;
        ret = msgbus_msg_envelope_get(m_meta_data, "additional_frames",
                                      &additional_frames_arr);
        if (ret != MSG_SUCCESS) {
            msgbus_msg_envelope_elem_destroy(e_width);
            msgbus_msg_envelope_elem_destroy(e_height);
            msgbus_msg_envelope_elem_destroy(e_channels);
            msgbus_msg_envelope_elem_destroy(additional_frames_arr);
            throw "Failed to fetch additional_frames object";
        }
        if (additional_frames_arr->type != MSG_ENV_DT_ARRAY) {
            throw "additional_frames type not an array";
        }
        // Remove existing blob at index and re-add data to the end
        msg_envelope_elem_body_t* obj = \
            msgbus_msg_envelope_elem_array_get_at(additional_frames_arr,
                                                  (index - 1));
        if (obj == NULL) {
            msgbus_msg_envelope_elem_destroy(obj);
            msgbus_msg_envelope_elem_destroy(e_width);
            msgbus_msg_envelope_elem_destroy(e_height);
            msgbus_msg_envelope_elem_destroy(e_channels);
            msgbus_msg_envelope_elem_destroy(additional_frames_arr);
            throw "Unable to fetch meta-data";
        }
        ret = msgbus_msg_envelope_elem_object_remove(obj, "width");
        if (ret != MSG_SUCCESS) {
            msgbus_msg_envelope_elem_destroy(obj);
            msgbus_msg_envelope_elem_destroy(e_width);
            msgbus_msg_envelope_elem_destroy(e_height);
            msgbus_msg_envelope_elem_destroy(e_channels);
            msgbus_msg_envelope_elem_destroy(additional_frames_arr);
            throw "Failed to remove width object";
        }
        ret = msgbus_msg_envelope_elem_object_remove(obj, "height");
        if (ret != MSG_SUCCESS) {
            msgbus_msg_envelope_elem_destroy(obj);
            msgbus_msg_envelope_elem_destroy(e_width);
            msgbus_msg_envelope_elem_destroy(e_height);
            msgbus_msg_envelope_elem_destroy(e_channels);
            msgbus_msg_envelope_elem_destroy(additional_frames_arr);
            throw "Failed to remove height object";
        }
        ret = msgbus_msg_envelope_elem_object_remove(obj, "channels");
        if (ret != MSG_SUCCESS) {
            msgbus_msg_envelope_elem_destroy(obj);
            msgbus_msg_envelope_elem_destroy(e_width);
            msgbus_msg_envelope_elem_destroy(e_height);
            msgbus_msg_envelope_elem_destroy(e_channels);
            msgbus_msg_envelope_elem_destroy(additional_frames_arr);
            throw "Failed to remove channels object";
        }

        msg_envelope_elem_body_t* e_width = \
            msgbus_msg_envelope_new_integer(width);
        if (e_width == NULL) {
            msgbus_msg_envelope_elem_destroy(obj);
            msgbus_msg_envelope_elem_destroy(e_width);
            msgbus_msg_envelope_elem_destroy(e_height);
            msgbus_msg_envelope_elem_destroy(e_channels);
            msgbus_msg_envelope_elem_destroy(additional_frames_arr);
            throw "Unable to fetch meta-data";
        }
        ret = msgbus_msg_envelope_elem_object_put(obj, "width", e_width);
        if (ret != MSG_SUCCESS) {
            msgbus_msg_envelope_elem_destroy(obj);
            msgbus_msg_envelope_elem_destroy(e_width);
            msgbus_msg_envelope_elem_destroy(e_height);
            msgbus_msg_envelope_elem_destroy(e_channels);
            msgbus_msg_envelope_elem_destroy(additional_frames_arr);
            throw "Unable to put width into meta-data";
        }
        msg_envelope_elem_body_t* e_height = \
            msgbus_msg_envelope_new_integer(height);
        if (e_height == NULL) {
            msgbus_msg_envelope_elem_destroy(obj);
            msgbus_msg_envelope_elem_destroy(e_width);
            msgbus_msg_envelope_elem_destroy(e_height);
            msgbus_msg_envelope_elem_destroy(e_channels);
            msgbus_msg_envelope_elem_destroy(additional_frames_arr);
            throw "Unable to fetch meta-data";
        }
        ret = msgbus_msg_envelope_elem_object_put(obj, "height", e_height);
        if (ret != MSG_SUCCESS) {
            msgbus_msg_envelope_elem_destroy(obj);
            msgbus_msg_envelope_elem_destroy(e_width);
            msgbus_msg_envelope_elem_destroy(e_height);
            msgbus_msg_envelope_elem_destroy(e_channels);
            msgbus_msg_envelope_elem_destroy(additional_frames_arr);
            throw "Unable to put height into meta-data";
        }
        msg_envelope_elem_body_t* e_channels = \
            msgbus_msg_envelope_new_integer(channels);
        if (e_channels == NULL) {
            msgbus_msg_envelope_elem_destroy(obj);
            msgbus_msg_envelope_elem_destroy(e_width);
            msgbus_msg_envelope_elem_destroy(e_height);
            msgbus_msg_envelope_elem_destroy(e_channels);
            msgbus_msg_envelope_elem_destroy(additional_frames_arr);
            throw "Unable to fetch meta-data";
        }
        ret = msgbus_msg_envelope_elem_object_put(obj, "channels", e_channels);
        if (ret != MSG_SUCCESS) {
            msgbus_msg_envelope_elem_destroy(obj);
            msgbus_msg_envelope_elem_destroy(e_width);
            msgbus_msg_envelope_elem_destroy(e_height);
            msgbus_msg_envelope_elem_destroy(e_channels);
            msgbus_msg_envelope_elem_destroy(additional_frames_arr);
            throw "Unable to put channels into meta-data";
        }
    }
}

void Frame::set_encoding(EncodeType encode_type, int encode_level) {
    if(!verify_encoding_level(encode_type, encode_level)) {
        throw "Invalid encoding level for the encoding type";
    }
    this->m_encode_type = encode_type;
    this->m_encode_level = encode_level;
    msg_envelope_elem_body_t* encoding_type = NULL;
    msg_envelope_elem_body_t* encoding_level = NULL;
    msgbus_ret_t ret;
    ret = msgbus_msg_envelope_get(
            m_meta_data, "encoding_type", &encoding_type);
    if(ret == MSG_SUCCESS) {
        ret = msgbus_msg_envelope_remove(m_meta_data, "encoding_type");
        if(ret != MSG_SUCCESS)
            throw "Failed to remove \"encoding_type\" from the meta-data";
    }

    ret = msgbus_msg_envelope_get(
            m_meta_data, "encoding_level", &encoding_level);
    if(ret == MSG_SUCCESS) {
        ret = msgbus_msg_envelope_remove(m_meta_data, "encoding_level");
        if(ret != MSG_SUCCESS)
            throw "Failed to remove \"encoding_level\" from the meta-data";
    }

    // Add encoding (if type is not NONE)
    if(encode_type != EncodeType::NONE) {
        msg_envelope_elem_body_t* e_enc_type = NULL;
        if(encode_type == EncodeType::JPEG) {
            e_enc_type = msgbus_msg_envelope_new_string("jpeg");
        } else {
            e_enc_type = msgbus_msg_envelope_new_string("png");
        }
        if(e_enc_type == NULL) {
            throw "Failed initialize encoding type meta-data";
        }
        msg_envelope_elem_body_t* e_enc_lvl = msgbus_msg_envelope_new_integer(
                encode_level);
        if(e_enc_lvl == NULL) {
            throw "Failed to initialize encoding level meta-data";
        }
        ret = msgbus_msg_envelope_put(m_meta_data, "encoding_type", e_enc_type);
        if(ret != MSG_SUCCESS) {
            throw "Failed to put encoding type in object";
        }
        ret = msgbus_msg_envelope_put(m_meta_data, "encoding_level", e_enc_lvl);
        if(ret != MSG_SUCCESS) {
            throw "Failed to put encoding level in object";
        }
    }
}

msg_envelope_t* Frame::get_meta_data() {
    if(m_serialized.load()) {
        LOG_ERROR_0("Cannot get meta-data after frame serialization");
        return NULL;
    }
    return m_meta_data;
}

void Frame::encode_frame() {
    LOG_DEBUG("Encoding the frame");
    for (int i = 0; i < m_num_frames; i++) {
        if (i == 0) {
            std::vector<uchar>* encoded_bytes = new std::vector<uchar>();
            std::vector<int> compression_params;
            std::string ext;

            // Build compression parameters
            switch(m_encode_type) {
                case EncodeType::JPEG:
                    ext = ".jpeg";
                    compression_params.push_back(cv::IMWRITE_JPEG_QUALITY);
                    break;
                case EncodeType::PNG:
                    ext = ".png";
                    compression_params.push_back(cv::IMWRITE_PNG_COMPRESSION);
                    break;
                case EncodeType::NONE:
                default:
                    delete encoded_bytes;
                    return;
            }

            // Add encoding level (value depends on encoding type)
            compression_params.push_back(m_encode_level);

            // Construct cv::Mat from our frame
            cv::Mat frame(m_height, m_width, CV_8UC(m_channels), m_data[0]);

            // Execute the encode
            bool success = cv::imencode(ext, frame, *encoded_bytes, compression_params);
            if(!success) {
                throw "Failed to encode the frame";
            }

            // Set the new data in the frame to correspond to the encoded buffer
            if (m_num_frames > 1) {
                // Setting free function to NULL since the entire frame and its
                // contents will be destroyed by free called for last frame
                this->set_data((void*) encoded_bytes, m_width, m_height, m_channels,
                            encoded_bytes->data(), NULL, 0);
            } else {
                this->set_data((void*) encoded_bytes, m_width, m_height, m_channels,
                            encoded_bytes->data(), free_encoded, 0);
            }
        } else {
            std::vector<uchar>* encoded_bytes = new std::vector<uchar>();
            std::vector<int> compression_params;
            std::string ext;

            // Build compression parameters
            switch(this->get_encode_type(i)) {
                case EncodeType::JPEG:
                    ext = ".jpeg";
                    compression_params.push_back(cv::IMWRITE_JPEG_QUALITY);
                    break;
                case EncodeType::PNG:
                    ext = ".png";
                    compression_params.push_back(cv::IMWRITE_PNG_COMPRESSION);
                    break;
                case EncodeType::NONE:
                default:
                    delete encoded_bytes;
                    return;
            }

            // Add encoding level (value depends on encoding type)
            compression_params.push_back(this->get_encode_level(i));

            // Construct cv::Mat from our frame
            cv::Mat frame(m_height, m_width, CV_8UC(m_channels), m_data[i]);

            // Execute the encode
            bool success = cv::imencode(ext, frame, *encoded_bytes, compression_params);
            if(!success) {
                throw "Failed to encode the frame";
            }

            // Set the new data in the frame to correspond to the encoded buffer
            if (i == (m_num_frames - 1)) {
                this->set_data((void*) encoded_bytes, m_width, m_height, m_channels,
                               encoded_bytes->data(), free_encoded, i);
            } else {
                // Setting free function to NULL since the entire frame and its
                // contents will be destroyed by free called for last frame
                this->set_data((void*) encoded_bytes, m_width, m_height, m_channels,
                               encoded_bytes->data(), NULL, i);
            }
        }
    }
}

msg_envelope_t* Frame::serialize() {
    if(m_serialized.load()) {
        LOG_ERROR_0("Frame has already been serialized");
        return NULL;
    }

    if(m_encode_type != EncodeType::NONE) {
        this->encode_frame();
    }

    if(m_msg != NULL) {
        // Message was deserialized
        // Set the frame as serialized
        m_serialized.store(true);

        // Get the data blob for the frame data from the envelope to set the
        // free function on the shared memory
        msg_envelope_elem_body_t* blob = NULL;
        msgbus_ret_t ret = msgbus_msg_envelope_get(m_msg, NULL, &blob);
        if(ret != MSG_SUCCESS) {
            throw "Failed to retrieve frame blob from  msg envelope";
        } else if(blob->type == MSG_ENV_DT_BLOB) {

            // Set the m_free_data member, because the next lines after m_free_data
            // is set swap around the blob pointer and free functions so that the
            // frame object is correctly destroyed along with the actual blob bytes
            this->m_blob_ptr = (owned_blob_t**)malloc(sizeof(owned_blob_t*));
            if (this->m_blob_ptr == NULL) {
                throw "malloc failed for m_blob_ptr";
            }
            this->m_blob_ptr[0] = owned_blob_copy(blob->body.blob->shared);
            if (this->m_blob_ptr[0] == NULL) {
                free(this->m_blob_ptr);
                throw "Failed to copy msg_envelope_t owned blob";
            }

            // Resetting blob shared memory free pointers
            blob->body.blob->shared->ptr = this;
            blob->body.blob->shared->free = Frame::msg_free_frame;
            blob->body.blob->shared->owned = true;

            // Set m_msg to NULL so that the Deserializable destructor does not
            // call msgbus_msg_envelope_destroy() on the m_msg object. The reason
            // we do not want that called is because the message bus itself will
            // call the destroy method on the message envelope when the envelope
            // has been transmitted over the message bus.
            m_msg = NULL;

            return m_meta_data;
        } else if (blob->type == MSG_ENV_DT_ARRAY) {
            int num_parts = blob->body.array->len;
            for (int i = 0; i < num_parts; i++) {
                msg_envelope_elem_body_t* arr_blob = \
                    msgbus_msg_envelope_elem_array_get_at(blob, i);

                // Set the m_free_data member, because the next lines after m_free_data
                // is set swap around the blob pointer and free functions so that the
                // frame object is correctly destroyed along with the actual blob bytes
                this->m_blob_ptr = (owned_blob_t**)malloc(sizeof(owned_blob_t*));
                if (this->m_blob_ptr == NULL) {
                    throw "malloc failed for m_blob_ptr";
                }
                this->m_blob_ptr[i] = owned_blob_copy(arr_blob->body.blob->shared);
                if (this->m_blob_ptr[i] == NULL) {
                    free(this->m_blob_ptr);
                    throw "Failed to copy msg_envelope_t owned blob";
                }

                // Resetting blob shared memory free pointers
                arr_blob->body.blob->shared->ptr = this;
                // Free this only for last blob and set to NULL for all other blobs
                if (i == (num_parts - 1)) {
                    arr_blob->body.blob->shared->free = Frame::msg_free_frame;
                    arr_blob->body.blob->shared->owned = true;
                } else {
                    arr_blob->body.blob->shared->free = NULL;
                    arr_blob->body.blob->shared->owned = false;
                }
            }
            m_msg = NULL;

            return m_meta_data;
        }
    } else {
        for (int i = 0; i < m_num_frames; i++) {
            msg_envelope_elem_body_t* frame = NULL;
            msgbus_ret_t ret = MSG_SUCCESS;

            int len = 0;
            if (i == 0) {
                len = m_width * m_height * m_channels;
                if (m_encode_type != EncodeType::NONE) {
                    len = ((std::vector<uchar>*) m_frame)->size();
                }
            } else {
                msg_envelope_elem_body_t* additional_frames_arr;
                ret = msgbus_msg_envelope_get(m_meta_data, "additional_frames",
                                              &additional_frames_arr);
                if (additional_frames_arr->type != MSG_ENV_DT_ARRAY) {
                    throw "additional_frames type not an array";
                }
                msg_envelope_elem_body_t* af_arr_obj = \
                    msgbus_msg_envelope_elem_array_get_at(additional_frames_arr, (i - 1));
                if (af_arr_obj == NULL) {
                    throw "af_arr_obj is NULL";
                }
                if (af_arr_obj->type != MSG_ENV_DT_OBJECT) {
                    throw "af_arr_obj type should be MSG_ENV_DT_OBJECT";
                }
                msg_envelope_elem_body_t* frame_width = \
                    msgbus_msg_envelope_elem_object_get(af_arr_obj, "width");
                if (frame_width == NULL || frame_width->type != MSG_ENV_DT_INT) {
                    throw "frame_width is NULL";
                }
                msg_envelope_elem_body_t* frame_height = \
                    msgbus_msg_envelope_elem_object_get(af_arr_obj, "height");
                if (frame_height == NULL || frame_height->type != MSG_ENV_DT_INT) {
                    throw "frame_height is NULL";
                }
                msg_envelope_elem_body_t* frame_channels = \
                    msgbus_msg_envelope_elem_object_get(af_arr_obj, "channels");
                if (frame_channels == NULL || frame_channels->type != MSG_ENV_DT_INT) {
                    throw "frame_channels is NULL";
                }
                len = frame_width->body.integer * frame_height->body.integer * frame_channels->body.integer;
            }

            frame = msgbus_msg_envelope_new_blob((char*) m_data[i], len);
            if (frame == NULL) {
                throw "Failed to initialize blob for frame data";
            }

            // Set the blob free methods
            frame->body.blob->shared->ptr = (void*) this;
            // Free this only for last blob and set to NULL for all other blobs
            if (i == m_num_frames-1) {
                frame->body.blob->shared->free = Frame::msg_free_frame;
            } else {
                frame->body.blob->shared->free = NULL;
            }

            ret = msgbus_msg_envelope_put(
                    m_meta_data, NULL, frame);
            if(ret != MSG_SUCCESS) {
                LOG_ERROR_0("Failed to put frame data into envelope");

                // Set owned flag to false, so that the frame's data is not
                // freed yet...
                frame->body.blob->shared->owned = false;
                msgbus_msg_envelope_elem_destroy(frame);

                return NULL;
            }
        }

        // Set the frame as serialized
        m_serialized.store(true);

        return m_meta_data;
    }
}

void free_decoded(void* varg) {
    // Does nothing... Since the memory is managed by the cv::Mat itself
    cv::Mat* frame = (cv::Mat*) varg;
    delete frame;
}

void free_encoded(void* varg) {
    std::vector<uchar>* encoded_bytes = (std::vector<uchar>*) varg;
    delete encoded_bytes;
}

bool verify_encoding_level(EncodeType encode_type, int encode_level) {
    switch(encode_type) {
        case EncodeType::JPEG: return encode_level >= 0 && encode_level <= 100;
        case EncodeType::PNG:  return encode_level >= 0 && encode_level <= 9;
        case EncodeType::NONE: return true;
        default:               return true;
    }
}

