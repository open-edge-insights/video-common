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
 * @brief Implementation of @c Frame class
 */

#include <sstream>
#include <random>
#include <vector>
#include <opencv2/opencv.hpp>
#include <safe_lib.h>
#include <eii/utils/logger.h>

#include "eii/udf/frame.h"

#define UUID_LENGTH 5

using namespace eii::udf;

// Prototyes
static void free_decoded(void* varg);
static void free_encoded(void* varg);
static void free_msg_env_blob(void* varg);
static void free_frame_data(void* varg);
static void free_frame_data_final(void* varg);
static cv::Mat* decode_frame(
        EncodeType encode_type, uchar* encoded_data, size_t len);
static void add_frame_meta_env(msg_envelope_t* env, FrameMetaData* meta);
static void add_frame_meta_obj(
        msg_envelope_elem_body_t* obj, FrameMetaData* meta);
static EncodeType str_to_encode_type(const char* value);
static std::string generate_image_handle(int len);

// Simple struct for use with free_frame_data_final()
class FinalFreeWrapper {
private:
    // Reference to the frame object to delete
    Frame* m_frame;

    // Reference to the final piece of frame data which needs to be deleted.
    FrameData* m_frame_data;

    /**
     * Private @c FinalFreeWrapper copy constructor.
     */
    FinalFreeWrapper(const FinalFreeWrapper& src);

    /**
     * Private @c FinalFreeWrapper assignment operator.
     */
    FinalFreeWrapper& operator=(const FinalFreeWrapper& src);

public:
    FinalFreeWrapper(Frame* frame, FrameData* fd) :
        m_frame(frame), m_frame_data(fd)
    {};

    ~FinalFreeWrapper() {
        delete m_frame_data;
        delete m_frame;
    };
};

FinalFreeWrapper::FinalFreeWrapper(const FinalFreeWrapper& src) {
    throw "This object should not be copied";
}

FinalFreeWrapper& FinalFreeWrapper::operator=(const FinalFreeWrapper& src) {
    return *this;
}

/**
 * Helper method to verify the correct encoding level is set for the given
 * encoding type.
 */
static bool verify_encoding_level(EncodeType encode_type, int encode_level);


Frame::Frame(
        void* frame, void (*free_frame)(void*), void* data,
        int width, int height, int channels, EncodeType encode_type,
        int encode_level) :
    Serializable(NULL), m_meta_data(NULL), m_additional_frames_arr(NULL),
    m_serialized(false)
{
    if(free_frame == NULL) {
        throw "The free_frame() method cannot be NULL";
    }
    if(!verify_encoding_level(encode_type, encode_level)) {
        throw "Encode level invalid for the encoding type";
    }

    // Generate image handle for the frame
    std::string img_handle = generate_image_handle(UUID_LENGTH);

    // TODO(kmidkiff): Image handle????
    FrameMetaData* meta = new FrameMetaData(
            img_handle, width, height, channels, encode_type, encode_level);
    FrameData* fd = new FrameData(frame, free_frame, data, meta);
    this->m_frames.push_back(fd);

    try {
        m_meta_data = msgbus_msg_envelope_new(CT_JSON);
        if(m_meta_data == NULL) {
            throw "Failed to initialize meta data envelope";
        }
        add_frame_meta_env(m_meta_data, meta);
    } catch (const char* ex) {
        // Free allocated memory in error case
        delete fd;
        msgbus_msg_envelope_destroy(m_meta_data);
        throw ex;
    }
}

Frame::Frame() :
    Serializable(NULL), m_meta_data(NULL), m_additional_frames_arr(NULL),
    m_serialized(false)
{
    m_meta_data = msgbus_msg_envelope_new(CT_JSON);
    if(m_meta_data == NULL) {
        throw "Failed to initialize meta data envelope";
    }
}

#define MSG_TYPE_STR(value) #value

void get_meta_from_env(
        msg_envelope_t* env, const char* key,
        msg_envelope_elem_body_t** dest, msg_envelope_data_type_t expected) {
    msgbus_ret_t ret = msgbus_msg_envelope_get(env, key, dest);
    if (ret != MSG_SUCCESS) {
        LOG_ERROR("Frame meta-data missing key: %s", key);
        throw "Failed to get meta-data key";
    } else if ((*dest)->type != expected) {
        LOG_ERROR("Incorrect meta-data type, expceted: %s, got: %s",
                MSG_TYPE_STR(expected), MSG_TYPE_STR((*dest)->type));
        throw "Meta-data incorrect type";
    }
}

void get_meta_from_obj(
        msg_envelope_elem_body_t* obj, const char* key,
        msg_envelope_elem_body_t** dest, msg_envelope_data_type_t expected) {
    (*dest) = msgbus_msg_envelope_elem_object_get(obj, key);
    if ((*dest) == NULL) {
        LOG_ERROR("Frame meta-data missing key: %s", key);
        throw "Failed to get meta-data key";
    } else if ((*dest)->type != expected) {
        LOG_ERROR("Incorrect meta-data type, expceted: %s, got: %s",
                MSG_TYPE_STR(expected), MSG_TYPE_STR((*dest)->type));
        throw "Meta-data incorrect type";
    }
}

Frame::Frame(msg_envelope_t* msg) :
    Serializable(NULL), m_meta_data(NULL), m_additional_frames_arr(NULL),
    m_serialized(false)
{
    // TODO(kmidkiff): VERIFY IT IS CT_JSON

    msgbus_ret_t ret = MSG_SUCCESS;
    msg_envelope_elem_body_t* blob = NULL;
    msg_envelope_elem_body_t* width = NULL;
    msg_envelope_elem_body_t* height = NULL;
    msg_envelope_elem_body_t* channels = NULL;
    msg_envelope_elem_body_t* enc_type = NULL;
    msg_envelope_elem_body_t* enc_lvl = NULL;
    msg_envelope_elem_body_t* img_handle = NULL;
    msg_envelope_elem_body_t* obj = NULL;
    EncodeType encode_type = EncodeType::NONE;

    ret = msgbus_msg_envelope_get(msg, NULL, &blob);
    if(ret != MSG_SUCCESS) {
        throw "Failed to retrieve frame blob from  msg envelope";
    }

    // After getting the blob from the serialize message, set the blob to
    // NULL in the envelope, that way this function has taken over the
    // ownership of the memory...
    msg->blob = NULL;

    m_meta_data = msg;

    int num_frames = 1;
    if (blob->type == MSG_ENV_DT_ARRAY) {
        num_frames = blob->body.array->len;
        get_meta_from_env(
                msg, "additional_frames", &m_additional_frames_arr,
                MSG_ENV_DT_ARRAY);
    }

    for (int i = 0; i < num_frames; i++) {
        msg_envelope_elem_body_t* frame = NULL;
        // Manually create a new blob
        msg_envelope_elem_body_t* elem = NULL;
        msg_envelope_blob_t* b = NULL;
        if (blob->type == MSG_ENV_DT_ARRAY) {
            LOG_DEBUG("GETTING FRAME BLOB: %d", i);
            frame = msgbus_msg_envelope_elem_array_get_at(blob, i);
            if (frame == NULL) {
                LOG_ERROR("No frame at index %d", i);
                throw "Failed to obtain frame from array";
            }

            frame->body.blob->shared->owned = false;

            owned_blob_t* shared = owned_blob_copy(frame->body.blob->shared);
            shared->owned = true;

            b = (msg_envelope_blob_t*) malloc(sizeof(msg_envelope_blob_t));
            if(b == NULL) {
                throw "Failed to initialize new blob";
            }
            b->shared = shared;
            b->len = shared->len;
            b->data = shared->bytes;

            elem = (msg_envelope_elem_body_t*) malloc(
                    sizeof(msg_envelope_elem_body_t));
            if(elem == NULL) {
                free(b);
                throw "Failed to initailize new element";
            }

            elem->type = MSG_ENV_DT_BLOB;
            elem->body.blob = b;

            frame = elem;
        } else {
            frame = blob;
        }

        if (i == 0) {
            get_meta_from_env(msg, "width", &width, MSG_ENV_DT_INT);
            get_meta_from_env(msg, "height", &height, MSG_ENV_DT_INT);
            get_meta_from_env(msg, "channels", &channels, MSG_ENV_DT_INT);

            // The following three meta-data items can be missing
            msgbus_msg_envelope_get(msg, "img_handle", &img_handle);
            msgbus_msg_envelope_get(msg, "encoding_type", &enc_type);
            msgbus_msg_envelope_get(msg, "encoding_level", &enc_lvl);
        } else {
            obj = msgbus_msg_envelope_elem_array_get_at(
                    m_additional_frames_arr, i - 1);
            if (obj == NULL) {
                free(b);
                msgbus_msg_envelope_elem_destroy(elem);
                throw "Failed to get additional array element";
            } else if (obj->type != MSG_ENV_DT_OBJECT) {
                free(b);
                msgbus_msg_envelope_elem_destroy(elem);
                throw "Additional array element must be objects";
            }

            get_meta_from_obj(obj, "width", &width, MSG_ENV_DT_INT);
            get_meta_from_obj(obj, "height", &height, MSG_ENV_DT_INT);
            get_meta_from_obj(obj, "channels", &channels, MSG_ENV_DT_INT);

            // The following three meta-data items can be missing
            img_handle = msgbus_msg_envelope_elem_object_get(
                    obj, "img_handle");
            enc_type = msgbus_msg_envelope_elem_object_get(
                    obj, "encoding_type");
            enc_lvl = msgbus_msg_envelope_elem_object_get(
                    obj, "encoding_level");
        }

        std::string img_handle_str = "";
        if (img_handle != NULL) {
            if (img_handle->type != MSG_ENV_DT_STRING) {
                free(b);
                msgbus_msg_envelope_elem_destroy(elem);
                throw "Image handle must be a string";
            }
            img_handle_str = std::string(img_handle->body.string);
        }

        if (enc_type != NULL) {
            if(enc_type->type != MSG_ENV_DT_STRING) {
                free(b);
                msgbus_msg_envelope_elem_destroy(elem);
                throw "Encoding type must be a string";
            } else if (enc_lvl == NULL) {
                free(b);
                msgbus_msg_envelope_elem_destroy(elem);
                throw "Missing encoding level";
            } else if (enc_lvl->type != MSG_ENV_DT_INT) {
                free(b);
                msgbus_msg_envelope_elem_destroy(elem);
                throw "Encoding level must be an integer";
            }

            // Parse encoding type (NOTE: Function call throws exceptions)
            encode_type = str_to_encode_type(enc_type->body.string);

            // Construct vector of bytes to pass to cv::imdecode
            uchar* buf = (uchar*) frame->body.blob->data;
            size_t len = (size_t) frame->body.blob->len;
            cv::Mat* decoded = decode_frame(encode_type, buf, len);

            // TODO(kmidkiff): Should check that the metadata is correct
            // with what was decoded
            FrameMetaData* meta = new FrameMetaData(
                    img_handle_str,
                    width->body.integer, height->body.integer,
                    channels->body.integer, encode_type,
                    enc_lvl->body.integer);
            FrameData* fd = new FrameData(
                    (void*) decoded, free_decoded,
                    (void*) decoded->data, meta);
            m_frames.push_back(fd);

            msgbus_msg_envelope_elem_destroy(frame);
        } else {
            // TODO(kmidkiff): This could modify meta-data if enc level was
            // still specified (but this would be incorrect data...)
            FrameMetaData* meta = new FrameMetaData(
                    img_handle_str,
                    width->body.integer, height->body.integer,
                    channels->body.integer, EncodeType::NONE, 0);
            FrameData* fd = new FrameData(
                    (void*) frame, free_msg_env_blob,
                    (void*) frame->body.blob->data, meta);
            m_frames.push_back(fd);
        }

        width = NULL;
        height = NULL;
        channels = NULL;
        enc_type = NULL;
        enc_lvl = NULL;
        frame = NULL;
    }

    if (blob->type == MSG_ENV_DT_ARRAY) {
        // Destroy the empty blob array
        msgbus_msg_envelope_elem_destroy(blob);
    }

    // TODO(kmidkiff): There could be a memory leak of FrameData added to the
    // m_frame vector if an error happens after one or more frames have already
    // been added...
}

Frame::Frame(const Frame& src) {
    throw "This object should not be copied";
}

Frame& Frame::operator=(const Frame& src) {
    return *this;
}

Frame::~Frame() {
    // All other memory after being serialized is no longer the Frame object's
    // responsibility to free
    if (!m_serialized.load()) {
        for (int i = 0; i < this->get_number_of_frames(); i++) {
            FrameData* fd = this->m_frames[i];
            delete fd;
        }
        msgbus_msg_envelope_destroy(m_meta_data);
    }
}

std::string Frame::get_img_handle(int index) {
    if (index > (int) m_frames.size()) {
        throw "Index out of range";
    }
    return m_frames[index]->get_meta_data()->get_img_handle();
}

int Frame::get_width(int index) {
    if (index > (int) m_frames.size()) {
        throw "Index out of range";
    }
    return m_frames[index]->get_meta_data()->get_width();
}

int Frame::get_height(int index) {
    if (index > (int) m_frames.size()) {
        throw "Index out of range";
    }
    return m_frames[index]->get_meta_data()->get_height();
}

int Frame::get_channels(int index) {
    if (index > (int) m_frames.size()) {
        throw "Index out of range";
    }
    return m_frames[index]->get_meta_data()->get_channels();
}

EncodeType Frame::get_encode_type(int index) {
    if (index > (int) m_frames.size()) {
        throw "Index out of range";
    }
    return m_frames[index]->get_meta_data()->get_encode_type();
}

int Frame::get_encode_level(int index) {
    if (index > (int) m_frames.size()) {
        throw "Index out of range";
    }
    return m_frames[index]->get_meta_data()->get_encode_level();
}

void* Frame::get_data(int index) {
    if(m_serialized.load()) {
        LOG_ERROR_0(
                "Writable data method called after frame serialization");
        return NULL;
    }
    if (index > (int) m_frames.size()) {
        throw "Index out of range";
    }
    return m_frames[index]->get_data();
}

int Frame::get_number_of_frames() {
    return (int) m_frames.size();
}

void Frame::add_frame(
        void* frame, void (*free_frame)(void*), void* data,
        int width, int height, int channels, EncodeType encode_type,
        int encode_level) {
    msgbus_ret_t ret = MSG_SUCCESS;
    msg_envelope_elem_body_t* obj = NULL;
    std::string img_handle = generate_image_handle(UUID_LENGTH);
    FrameMetaData* meta = new FrameMetaData(
            img_handle, width, height, channels, encode_type, encode_level);

    try {
        if (this->get_number_of_frames() == 0) {
            // Adding the first frame to the frame object
            add_frame_meta_env(m_meta_data, meta);
        } else {
            msg_envelope_elem_body_t* obj = msgbus_msg_envelope_new_object();
            if (obj == NULL) {
                delete meta;
                throw "Failed to initialize message envelope object";
            }
            add_frame_meta_obj(obj, meta);

            if (m_additional_frames_arr == NULL) {
                // Need to create the additional array
                m_additional_frames_arr = msgbus_msg_envelope_new_array();
                if (m_additional_frames_arr == NULL) {
                    delete meta;
                    throw "Failed to initialize additional_frames array";
                }

                // Add here so that the meta-data is not changed if adding the
                // object to the array fails
                ret = msgbus_msg_envelope_elem_array_add(
                        m_additional_frames_arr, obj);
                if (ret != MSG_SUCCESS) {
                    msgbus_msg_envelope_elem_destroy(m_additional_frames_arr);
                    m_additional_frames_arr = NULL;
                    delete meta;
                    throw "Failed to add meta object to array";
                }

                ret = msgbus_msg_envelope_put(
                        m_meta_data, "additional_frames",
                        m_additional_frames_arr);
                if (ret != MSG_SUCCESS) {
                    msgbus_msg_envelope_elem_destroy(m_additional_frames_arr);
                    m_additional_frames_arr = NULL;
                    delete meta;
                    throw "Failed to add additional frames array to meta-data";
                }
            } else {
                // No free needed for the array, because it previously existed
                // in the meta data envelope, if this fails, no changes will
                // have occurred to the underlying meta-data
                ret = msgbus_msg_envelope_elem_array_add(
                        m_additional_frames_arr, obj);
                if (ret != MSG_SUCCESS) {
                    delete meta;
                    throw "Failed to add meta object to array";
                }
            }
        }

        // Add the frame
        FrameData* fd = new FrameData(frame, free_frame, data, meta);
        this->m_frames.push_back(fd);
    } catch (const char* ex) {
        delete meta;
        if (obj != NULL) {
            msgbus_msg_envelope_elem_destroy(obj);
        }
        throw ex;
    }
}

// Helper defines to remove frame meta data from the root meta-data message
// envelope or from the additional_frames meta-data array object.
#define REMOVE_META(env, key) { \
    ret = msgbus_msg_envelope_remove(env, key); \
    if (ret != MSG_SUCCESS && ret != MSG_ERR_ELEM_NOT_EXIST) { \
        LOG_ERROR("[%d] Failed to remove meta data: %s", ret,  key); \
        throw "Failed to remove old meta-data key from envelope"; \
    } \
}
#define REMOVE_META_OBJ(obj, key) { \
    ret = msgbus_msg_envelope_elem_object_remove(obj, key); \
    if (ret != MSG_SUCCESS && ret != MSG_ERR_ELEM_NOT_EXIST) { \
        LOG_ERROR("[%d] Failed to remove meta data: %s", ret,  key); \
        throw "Failed to remove old meta-data key from object"; \
    } \
}

void Frame::set_data(
        int index, void* frame, void (*free_frame)(void*), void* data,
        int width, int height, int channels)
{
    msgbus_ret_t ret = MSG_SUCCESS;

    if (index > this->get_number_of_frames() - 1) {
        throw "Index out-of-range";
    }

    if (m_serialized.load()) {
        LOG_ERROR_0("Cannot set data after serialization");
        throw "Cannot set data after serialization";
    }


    // Replace the old frame data in m_frames and delete the old frame data
    FrameData* old_fd = this->m_frames[index];
    FrameMetaData* old_meta = old_fd->get_meta_data();

    // Constructing the new FrameData
    FrameMetaData* new_meta = new FrameMetaData(
            old_meta->get_img_handle(),
            width, height, channels,
            old_meta->get_encode_type(),
            old_meta->get_encode_level());
    FrameData* new_fd = new FrameData(frame, free_frame, data, new_meta);

    this->m_frames[index] = new_fd;

    // Update the meta-data
    // NOTE: An error here is somewhat irrecoverable
    try {
        if (index == 0) {

            // Remove old values
            REMOVE_META(m_meta_data, "img_handle");
            REMOVE_META(m_meta_data, "width");
            REMOVE_META(m_meta_data, "height");
            REMOVE_META(m_meta_data, "channels");
            REMOVE_META(m_meta_data, "encoding_type");
            REMOVE_META(m_meta_data, "encoding_level");

            // Add the new meta-data values
            add_frame_meta_env(m_meta_data, new_meta);
        } else {
            // Update meta-data in additional frames list
            // NOTE: This is just a sanity check, THIS SHOULD NEVER HAPPEN
            assert(m_additional_frames_arr != NULL);

            // Construct new meta-data object
            //
            // NOTE: Getting index - 1, because index - is technically at the
            // root level of the frame meta-data in the msg_envelope_t. In the
            // additional frames meta-data, index "0" is technically "1".
            msg_envelope_elem_body_t* obj =
                msgbus_msg_envelope_elem_array_get_at(
                    m_additional_frames_arr, index - 1);
            if (obj == NULL) {
                throw "Failed to get meta-data object";
            }

            // Remove old values
            REMOVE_META_OBJ(obj, "img_handle");
            REMOVE_META_OBJ(obj, "width");
            REMOVE_META_OBJ(obj, "height");
            REMOVE_META_OBJ(obj, "channels");
            REMOVE_META_OBJ(obj, "encoding_type");
            REMOVE_META_OBJ(obj, "encoding_level");

            // Add the new meta-data values
            add_frame_meta_obj(obj, new_meta);
        }

        // Release old data
        delete old_fd;
    } catch (const char* ex) {
        delete old_fd;
        throw ex;
    }
}

void Frame::set_encoding(EncodeType encode_type, int encode_level, int index) {
    msgbus_ret_t ret = MSG_SUCCESS;
    msg_envelope_elem_body_t* obj = NULL;

    if(!verify_encoding_level(encode_type, encode_level)) {
        throw "Invalid encoding level for the encoding type";
    }

    if (index > this->get_number_of_frames() - 1) {
        throw "Index out-of-range";
    }

    if (index != 0) {
        obj = msgbus_msg_envelope_elem_array_get_at(
                    m_additional_frames_arr, index);
        if (obj == NULL) {
            throw "Failed to get meta-data object for additional frame";
        }
    }

    FrameMetaData* meta = this->m_frames[index]->get_meta_data();

    if (meta->get_encode_type() != EncodeType::NONE) {
        if (index == 0) {
            REMOVE_META(m_meta_data, "encoding_type");
            REMOVE_META(m_meta_data, "encoding_level");
        } else {
            REMOVE_META_OBJ(obj, "encoding_type");
            REMOVE_META_OBJ(obj, "encoding_level");
        }
    }

    // Set encoding on the meta data
    meta->set_encoding(encode_type, encode_level);

    if (encode_type != EncodeType::NONE) {
        msg_envelope_elem_body_t* enc_type = NULL;
        msg_envelope_elem_body_t* enc_level =
            msgbus_msg_envelope_new_integer(encode_level);
        if (enc_level == NULL) {
            throw "Failed to intialize new envelope integer";
        }

        if(encode_type == EncodeType::JPEG) {
            enc_type = msgbus_msg_envelope_new_string("jpeg");
        } else {
            enc_type = msgbus_msg_envelope_new_string("png");
        }
        if(enc_type == NULL) {
            msgbus_msg_envelope_elem_destroy(enc_level);
            throw "Failed initialize encoding type meta-data";
        }

        if (index == 0) {
            ret = msgbus_msg_envelope_put(
                    m_meta_data, "encoding_type", enc_type);
            if (ret != MSG_SUCCESS) {
                msgbus_msg_envelope_elem_destroy(enc_level);
                msgbus_msg_envelope_elem_destroy(enc_type);
                throw "Failed to put \"encoding_type\" in envelope";
            }

            ret = msgbus_msg_envelope_put(
                    m_meta_data, "encoding_level", enc_level);
            if (ret != MSG_SUCCESS) {
                msgbus_msg_envelope_elem_destroy(enc_level);
                throw "Failed to put \"encoding_type\" in envelope";
            }
        } else {
            ret = msgbus_msg_envelope_elem_object_put(
                    obj, "encoding_type", enc_type);
            if (ret != MSG_SUCCESS) {
                msgbus_msg_envelope_elem_destroy(enc_level);
                msgbus_msg_envelope_elem_destroy(enc_type);
                throw "Failed to put \"encoding_type\" in object";
            }

            ret = msgbus_msg_envelope_elem_object_put(
                    obj, "encoding_level", enc_level);
            if (ret != MSG_SUCCESS) {
                msgbus_msg_envelope_elem_destroy(enc_level);
                throw "Failed to put \"encoding_type\" in object";
            }
        }
    } else {
        LOG_DEBUG("Removed encoding for frame: %d", index);
    }
}

msg_envelope_t* Frame::get_meta_data() {
    if (m_serialized.load()) {
        LOG_ERROR_0("Cannot get meta-data after frame serialization");
        return NULL;
    }
    return m_meta_data;
}

msg_envelope_t* Frame::serialize() {
    msg_envelope_elem_body_t* blob = NULL;
    msgbus_ret_t ret = MSG_SUCCESS;
    FrameData* fd = NULL;

    if(m_serialized.load()) {
        LOG_ERROR_0("Frame has already been serialized");
        return NULL;
    }

    // NOTE: Irrecoverable if an error occurs
    m_serialized.store(true);

    // Add all frames as blobs to the message envelope
    for (int i = 0; i < this->get_number_of_frames(); i++) {
        fd = this->m_frames[i];
        fd->encode();

        blob = msgbus_msg_envelope_new_blob(
                (char*) fd->get_data(), fd->get_size());
        if (blob == NULL) {
            // TODO(kmidkiff): Need to manage memory freeing for this error
            LOG_ERROR_0("Failed to initialize new blob");
            throw "Failed to initialize new blob";
        }

        // Set the ownership principles
        blob->body.blob->shared->ptr = (void*) fd;
        blob->body.blob->shared->owned = true;
        if (i == (this->get_number_of_frames() - 1)) {
            // Last blob is responsible for making sure the Frame object
            // itself also gets deleted
            FinalFreeWrapper* ffw = new FinalFreeWrapper(this, fd);
            blob->body.blob->shared->ptr = (void*) ffw;
            blob->body.blob->shared->free = free_frame_data_final;
        } else {
            blob->body.blob->shared->free = free_frame_data;
        }

        ret = msgbus_msg_envelope_put(m_meta_data, NULL, blob);
        if (ret != MSG_SUCCESS) {
            // TODO(kmidkiff): Need to manage freeing of memory for this error
            LOG_ERROR("Failed to put blob: %d", ret);
            throw "Failed to add blob to message envelope";
        }
        blob = NULL;
    }

    msg_envelope_t* msg = m_meta_data;
    m_meta_data = NULL;
    m_frames.clear();

    return msg;
}

static void free_decoded(void* varg) {
    // Does nothing... Since the memory is managed by the cv::Mat itself
    cv::Mat* frame = (cv::Mat*) varg;
    delete frame;
}

static void free_encoded(void* varg) {
    std::vector<uchar>* encoded_bytes = (std::vector<uchar>*) varg;
    delete encoded_bytes;
}

static void free_msg_env_blob(void* varg) {
    msg_envelope_elem_body_t* elem = (msg_envelope_elem_body_t*) varg;
    msgbus_msg_envelope_elem_destroy(elem);
}

static void free_frame_data(void* varg) {
    FrameData* fd = (FrameData*) varg;
    delete fd;
}


static void free_frame_data_final(void* varg) {
    FinalFreeWrapper* ffw = (FinalFreeWrapper*) varg;
    delete ffw;
}

static void add_frame_meta_env(msg_envelope_t* env, FrameMetaData* meta) {
    msg_envelope_elem_body_t* e_width = NULL;
    msg_envelope_elem_body_t* e_height = NULL;
    msg_envelope_elem_body_t* e_channels = NULL;
    msg_envelope_elem_body_t* e_enc_type = NULL;
    msg_envelope_elem_body_t* e_enc_lvl = NULL;
    msg_envelope_elem_body_t* e_img_handle = NULL;
    msgbus_ret_t ret = MSG_SUCCESS;

    try {
        // Add frame details to meta data
        e_img_handle = msgbus_msg_envelope_new_string(
                meta->get_img_handle().c_str());
        if (e_img_handle == NULL) {
            throw "Failed to initialize img_handle meta-data";
        }
        ret = msgbus_msg_envelope_put(env, "img_handle", e_img_handle);
        if (ret != MSG_SUCCESS) {
            throw "Failed to put img_handle meta-data";
        }
        e_img_handle = NULL;

        e_width = msgbus_msg_envelope_new_integer(meta->get_width());
        if(e_width == NULL) {
            throw "Failed to initialize width meta-data";
        }
        ret = msgbus_msg_envelope_put(env, "width", e_width);
        if(ret != MSG_SUCCESS) {
            LOG_ERROR("Failed to put width meta-data: %d", ret);
            throw "Failed to put width meta-data";
        }

        // Setting to NULL because the envelope owns the memory now, if an
        // error happens after this, then it does not need to be freed in the
        // error case (because when the env is freed it will be freed).
        e_width = NULL;

        // Add height
        e_height = msgbus_msg_envelope_new_integer(meta->get_height());
        if(e_height == NULL) {
            throw "Failed to initialize height meta-data";
        }
        ret = msgbus_msg_envelope_put(env, "height", e_height);
        if(ret != MSG_SUCCESS) {
            throw "Failed to put height meta-data";
        }
        e_height = NULL;

        // Add channels
        e_channels = msgbus_msg_envelope_new_integer(meta->get_channels());
        if(e_channels == NULL) {
            throw "Failed to initialize channels meta-data";
        }
        ret = msgbus_msg_envelope_put(env, "channels", e_channels);
        if(ret != MSG_SUCCESS) {
            throw "Failed to put channels meta-data";
        }
        e_channels = NULL;

        // Add encoding (if type is not NONE)
        if(meta->get_encode_type() != EncodeType::NONE) {
            if(meta->get_encode_type() == EncodeType::JPEG) {
                e_enc_type = msgbus_msg_envelope_new_string("jpeg");
            } else {
                e_enc_type = msgbus_msg_envelope_new_string("png");
            }
            if(e_enc_type == NULL) {
                throw "Failed initialize encoding type meta-data";
            }
            ret = msgbus_msg_envelope_put(env, "encoding_type", e_enc_type);
            if(ret != MSG_SUCCESS) {
                throw "Failed to put encoding type in object";
            }
            e_enc_type = NULL;

            e_enc_lvl = msgbus_msg_envelope_new_integer(
                    meta->get_encode_level());
            if(e_enc_lvl == NULL) {
                throw "Failed to initialize encoding level meta-data";
            }
            ret = msgbus_msg_envelope_put(env, "encoding_level", e_enc_lvl);
            if(ret != MSG_SUCCESS) {
                throw "Failed to put encoding level in object";
            }
            e_enc_lvl = NULL;
        }
    } catch (const char* ex) {
        // Free allocated memory
        if (e_img_handle != NULL) {
            msgbus_msg_envelope_elem_destroy(e_img_handle);
        }
        if (e_width != NULL) {
            msgbus_msg_envelope_elem_destroy(e_width);
        }
        if (e_height != NULL) {
            msgbus_msg_envelope_elem_destroy(e_height);
        }
        if (e_channels != NULL) {
            msgbus_msg_envelope_elem_destroy(e_channels);
        }
        if (e_enc_type != NULL) {
            msgbus_msg_envelope_elem_destroy(e_enc_type); }
        if (e_enc_lvl != NULL) {
            msgbus_msg_envelope_elem_destroy(e_enc_lvl);
        }

        // Re-throw the exception
        throw ex;
    }
}

static void add_frame_meta_obj(
        msg_envelope_elem_body_t* obj, FrameMetaData* meta) {
    msg_envelope_elem_body_t* e_width = NULL;
    msg_envelope_elem_body_t* e_height = NULL;
    msg_envelope_elem_body_t* e_channels = NULL;
    msg_envelope_elem_body_t* e_enc_type = NULL;
    msg_envelope_elem_body_t* e_enc_lvl = NULL;
    msg_envelope_elem_body_t* e_img_handle = NULL;
    msgbus_ret_t ret = MSG_SUCCESS;

    try {
        // Add frame details to meta data
        e_img_handle = msgbus_msg_envelope_new_string(
                meta->get_img_handle().c_str());
        if (e_img_handle == NULL) {
            throw "Failed to initialize img_handle meta-data";
        }
        ret = msgbus_msg_envelope_elem_object_put(
                obj, "img_handle", e_img_handle);
        if (ret != MSG_SUCCESS) {
            throw "Failed to put img_handle meta-data";
        }
        e_img_handle = NULL;

        e_width = msgbus_msg_envelope_new_integer(meta->get_width());
        if(e_width == NULL) {
            throw "Failed to initialize width meta-data";
        }
        ret = msgbus_msg_envelope_elem_object_put(obj, "width", e_width);
        if(ret != MSG_SUCCESS) {
            throw "Failed to put width meta-data";
        }

        // Setting to NULL because the envelope owns the memory now, if an
        // error happens after this, then it does not need to be freed in the
        // error case (because when the env is freed it will be freed).
        e_width = NULL;

        // Add height
        e_height = msgbus_msg_envelope_new_integer(meta->get_height());
        if(e_height == NULL) {
            throw "Failed to initialize height meta-data";
        }
        ret = msgbus_msg_envelope_elem_object_put(obj, "height", e_height);
        if(ret != MSG_SUCCESS) {
            throw "Failed to put height meta-data";
        }
        e_height = NULL;

        // Add channels
        e_channels = msgbus_msg_envelope_new_integer(meta->get_channels());
        if(e_channels == NULL) {
            throw "Failed to initialize channels meta-data";
        }
        ret = msgbus_msg_envelope_elem_object_put(obj, "channels", e_channels);
        if(ret != MSG_SUCCESS) {
            throw "Failed to put channels meta-data";
        }
        e_channels = NULL;

        if (meta->get_encode_type() != EncodeType::NONE) {
            if(meta->get_encode_type() == EncodeType::JPEG) {
                e_enc_type = msgbus_msg_envelope_new_string("jpeg");
            } else {
                e_enc_type = msgbus_msg_envelope_new_string("png");
            }
            if(e_enc_type == NULL) {
                throw "Failed initialize encoding type meta-data";
            }
            ret = msgbus_msg_envelope_elem_object_put(
                    obj, "encoding_type", e_enc_type);
            if(ret != MSG_SUCCESS) {
                throw "Failed to put encoding type in object";
            }
            e_enc_type = NULL;

            e_enc_lvl = msgbus_msg_envelope_new_integer(
                    meta->get_encode_level());
            if(e_enc_lvl == NULL) {
                throw "Failed to initialize encoding level meta-data";
            }
            ret = msgbus_msg_envelope_elem_object_put(
                    obj, "encoding_level", e_enc_lvl);
            if(ret != MSG_SUCCESS) {
                throw "Failed to put encoding level in object";
            }
            e_enc_lvl = NULL;
        }
    } catch (const char* ex) {
        // Free allocated memory
        if (e_img_handle != NULL) {
            msgbus_msg_envelope_elem_destroy(e_img_handle);
        }
        if (e_width != NULL) {
            msgbus_msg_envelope_elem_destroy(e_width);
        }
        if (e_height != NULL) {
            msgbus_msg_envelope_elem_destroy(e_height);
        }
        if (e_channels != NULL) {
            msgbus_msg_envelope_elem_destroy(e_channels);
        }
        if (e_enc_type != NULL) {
            msgbus_msg_envelope_elem_destroy(e_enc_type);
        }
        if (e_enc_lvl != NULL) {
            msgbus_msg_envelope_elem_destroy(e_enc_lvl);
        }

        // Re-throw the exception
        throw ex;
    }
}


static bool verify_encoding_level(EncodeType encode_type, int encode_level) {
    switch(encode_type) {
        case EncodeType::JPEG: return encode_level >= 0 && encode_level <= 100;
        case EncodeType::PNG:  return encode_level >= 0 && encode_level <= 9;
        case EncodeType::NONE: return true;
        default:               return true;
    }
}

static cv::Mat* decode_frame(
        EncodeType encode_type, uchar* encoded_data, size_t len) {
    // Construct vector of bytes to pass to cv::imdecode
    std::vector<uchar> data;
    data.assign(encoded_data, encoded_data + len);

    // Decode the frame
    // TODO: Should we always decode as color?
    cv::Mat* decoded = new cv::Mat();
    *decoded = cv::imdecode(data, cv::IMREAD_COLOR);
    if(decoded->empty()) {
        delete decoded;
        throw "Failed to decode the encoded frame";
    }
    data.clear();

    return decoded;
}

FrameMetaData::FrameMetaData(
        std::string img_handle, int width, int height, int channels,
        EncodeType encode_type, int encode_level) :
    m_img_handle(img_handle), m_width(width), m_height(height),
    m_channels(channels), m_encode_type(encode_type),
    m_encode_level(encode_level)
{
    if (!verify_encoding_level(encode_type, encode_level)) {
        throw "Invalid encode type/level combination";
    }
}

FrameMetaData::FrameMetaData(const FrameMetaData& src) {
    throw "This object should not be copied";
}

FrameMetaData& FrameMetaData::operator=(const FrameMetaData& src) {
    return *this;
}

FrameMetaData::~FrameMetaData() {}

void FrameMetaData::set_width(int width) { m_width = width; }
void FrameMetaData::set_height(int height) { m_height = height; }
void FrameMetaData::set_channels(int channels) { m_channels = channels; }
void FrameMetaData::set_encoding(EncodeType encode_type, int encode_level) {
    if (!verify_encoding_level(encode_type, encode_level)) {
        throw "Invalid encoding type/level";
    }
    m_encode_type = encode_type;
    m_encode_level = encode_level;
}

std::string FrameMetaData::get_img_handle() { return m_img_handle; }
int FrameMetaData::get_width() { return m_width; }
int FrameMetaData::get_height() { return m_height; }
int FrameMetaData::get_channels() { return m_channels; }
EncodeType FrameMetaData::get_encode_type() { return m_encode_type; }
int FrameMetaData::get_encode_level() { return m_encode_level; }

FrameData::FrameData(
        void* frame, void (*free_frame)(void*), void* data,
        FrameMetaData* meta) :
    m_meta(meta), m_frame(frame), m_data(data), m_free_frame(free_frame)
{
    m_size = meta->get_width() * meta->get_height() * meta->get_channels();
}

FrameData::FrameData(const FrameData& src) {
    throw "This object should not be copied";
}

FrameData& FrameData::operator=(const FrameData& src) {
    return *this;
}

FrameData::~FrameData() {
    delete m_meta;
    if (this->m_free_frame != NULL) {
        this->m_free_frame(this->m_frame);
    }
}

FrameMetaData* FrameData::get_meta_data() { return m_meta; }
void* FrameData::get_data() { return m_data; }
size_t FrameData::get_size() { return m_size; }

void FrameData::encode() {
    std::vector<uchar>* encoded_bytes = new std::vector<uchar>();
    std::vector<int> compression_params;
    std::string ext;

    // Build compression parameters
    switch(m_meta->get_encode_type()) {
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
    compression_params.push_back(m_meta->get_encode_level());

    // Construct cv::Mat from our frame
    cv::Mat frame(
            m_meta->get_height(), m_meta->get_width(),
            CV_8UC(m_meta->get_channels()), m_data);

    // Execute the encode
    bool ret = cv::imencode(ext, frame, *encoded_bytes, compression_params);
    if(!ret) {
        delete encoded_bytes;
        throw "Failed to encode the frame";
    }

    // Free old data
    if (this->m_free_frame != NULL) {
        this->m_free_frame(this->m_frame);
    }

    // Setup new internal state
    this->m_frame = (void*) encoded_bytes;
    this->m_data = (void*) encoded_bytes->data();
    this->m_free_frame = free_encoded;
    this->m_size = encoded_bytes->size();
}

static EncodeType str_to_encode_type(const char* val) {
    int ind_jpeg = 0;
    int ind_png = 0;
    size_t len = strlen(val);

    strcmp_s(val, len, "jpeg", &ind_jpeg);
    strcmp_s(val, len, "png", &ind_png);

    if (ind_jpeg == 0) { return EncodeType::JPEG; }
    if (ind_png == 0) { return EncodeType::PNG; }

    throw "Unknown encode type";
}

static std::string generate_image_handle(int len) {
    std::stringstream ss;
    for (auto i = 0; i < len; i++) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);
        const auto rc = dis(gen);
        std::stringstream hexstream;
        hexstream << std::hex << rc;
        auto hex = hexstream.str();
        ss << (hex.length() < 2 ? '0' + hex : hex);
    }
    return ss.str();
}
