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
 * @brief Unit tests for the @c Frame object
 */

#include <opencv2/opencv.hpp>
#include <gtest/gtest.h>
#include <eii/utils/logger.h>
#include "eii/udf/frame.h"

using namespace eii::udf;

#define ASSERT_NULL(val) { \
    if(val != NULL) FAIL() << "Value should be NULL"; \
}

#define ASSERT_NOT_NULL(val) { \
    if(val == NULL) FAIL() << "Value shoud not be NULL"; \
}

//
// Helper objects
//

/**
 * Test object to represent a video frame
 */
class TestFrame {
public:
    char* data;

    TestFrame(char* data) : data(data)
    {};

    ~TestFrame() { delete data; };
};

void test_frame_free(void* hint) {
    TestFrame* tf = (TestFrame*) hint;
    if (tf != NULL) {
        delete tf;
    }
}

Frame* init_frame() {

    char* data = new char[14];
    memcpy(data, "Hello, World!", 14);

    TestFrame* tf = new TestFrame(data);

    Frame* frame = new Frame(
            (void*) tf, test_frame_free, (void*) data, 14, 1, 1);

    return frame;
}

Frame* init_multi_frame() {
    char* data1 = new char[14];
    char* data2 = new char[14];
    memcpy(data1, "Hello, World1", 14);
    memcpy(data2, "Hello, World2", 14);

    TestFrame* tf1 = new TestFrame(data1);
    TestFrame* tf2 = new TestFrame(data2);

    Frame* frame = new Frame();
    frame->add_frame(
            (void*) tf1, test_frame_free, (void*) data1, 14, 1, 1,
            EncodeType::NONE, 0);
    frame->add_frame(
            (void*) tf2, test_frame_free, (void*) data2, 14, 1, 1,
            EncodeType::NONE, 0);

    return frame;
}

// Test class definition for doing setup
class frame_tests : public ::testing::Test {
protected:
    void SetUp() override {
        set_log_level(LOG_LVL_DEBUG);
    }
};

/**
 * Free method to free the underlying @c cv::Mat wrapped by the udf::Frame
 * object.
 */
void free_cv_frame(void* frame) {
    LOG_DEBUG_0("Freeing load-example frame");
    cv::Mat* mat = (cv::Mat*) frame;
    delete mat;
}

//
// Tests
//

// Basic sanity test for initializing a and deleting a frame before
// serialization
TEST_F(frame_tests, basic_init) {
    Frame* frame = init_frame();

    ASSERT_EQ(frame->get_width(), 14);
    ASSERT_EQ(frame->get_height(), 1);
    ASSERT_EQ(frame->get_channels(), 1);

    delete frame;
}

// Basic test for serializing a frame and verifying the meta-data and blob of
// the serialized frame
TEST_F(frame_tests, basic_serialize_free) {
    Frame* frame = init_frame();
    msg_envelope_t* msg = frame->serialize();

    // Verify that the methods for the frame return NULL after serialization
    ASSERT_NULL(frame->get_meta_data());
    ASSERT_NULL(frame->serialize());
    ASSERT_NULL(frame->get_data(0));

    // Verify the meta-data exists
    msg_envelope_elem_body_t* w;
    msgbus_ret_t ret = msgbus_msg_envelope_get(msg, "width", &w);
    ASSERT_EQ(ret, MSG_SUCCESS);
    ASSERT_EQ(w->type, MSG_ENV_DT_INT);
    ASSERT_EQ(w->body.integer, 14);

    msg_envelope_elem_body_t* h;
    ret = msgbus_msg_envelope_get(msg, "height", &h);
    ASSERT_EQ(ret, MSG_SUCCESS);
    ASSERT_EQ(h->type, MSG_ENV_DT_INT);
    ASSERT_EQ(h->body.integer, 1);

    msg_envelope_elem_body_t* c;
    ret = msgbus_msg_envelope_get(msg, "channels", &c);
    ASSERT_EQ(ret, MSG_SUCCESS);
    ASSERT_EQ(c->type, MSG_ENV_DT_INT);
    ASSERT_EQ(c->body.integer, 1);

    // Verify the blob is the correct data
    msg_envelope_elem_body_t* blob;
    ret = msgbus_msg_envelope_get(msg, NULL, &blob);
    ASSERT_EQ(ret, MSG_SUCCESS);
    ASSERT_EQ(blob->type, MSG_ENV_DT_BLOB);
    printf("blob data %s\n", blob->body.blob->data);
    ASSERT_EQ(strcmp(blob->body.blob->data, "Hello, World!"), 0);

    msgbus_msg_envelope_destroy(msg);
    delete frame;
}

// Test that the modification of the data works correctly
TEST_F(frame_tests, modify_data) {
    Frame* frame = init_frame();

    // Modifying the data
    char* data = (char*) frame->get_data(0);
    memcpy(data, "Goodbye", 8);
    data[7] = '\0';

    // Add meta-data
    msg_envelope_t* meta = frame->get_meta_data();

    msg_envelope_elem_body_t* add = msgbus_msg_envelope_new_string("test");
    ASSERT_NOT_NULL(add);

    msgbus_ret_t ret = msgbus_msg_envelope_put(meta, "ADDED", add);
    ASSERT_EQ(ret, MSG_SUCCESS);

    // Serialize and verify the message
    msg_envelope_t* msg = frame->serialize();

    msg_envelope_elem_body_t* a;
    ret = msgbus_msg_envelope_get(msg, "ADDED", &a);
    ASSERT_EQ(ret, MSG_SUCCESS);
    ASSERT_EQ(a->type, MSG_ENV_DT_STRING);
    ASSERT_EQ(strcmp(a->body.string, "test"), 0);

    // Verify the blob is the correct data
    msg_envelope_elem_body_t* blob;
    ret = msgbus_msg_envelope_get(msg, NULL, &blob);
    ASSERT_EQ(ret, MSG_SUCCESS);
    ASSERT_EQ(blob->type, MSG_ENV_DT_BLOB);
    ASSERT_EQ(strcmp(blob->body.blob->data, "Goodbye"), 0);

    msgbus_msg_envelope_destroy(msg);
    delete frame;
}

// Basic deserialization test
TEST_F(frame_tests, basic_deserialize) {
    // Create initial message envelope (represents a message received over the
    // message bus)
    msg_envelope_t* env = msgbus_msg_envelope_new(CT_JSON);
    ASSERT_NOT_NULL(env);

    // Add basic meta-data
    msg_envelope_elem_body_t* w = msgbus_msg_envelope_new_integer(14);
    ASSERT_NOT_NULL(w);
    msgbus_ret_t ret = msgbus_msg_envelope_put(env, "width", w);
    ASSERT_EQ(ret, MSG_SUCCESS);
    msg_envelope_elem_body_t* h = msgbus_msg_envelope_new_integer(1);
    ASSERT_NOT_NULL(h);
    ret = msgbus_msg_envelope_put(env, "height", h);
    ASSERT_EQ(ret, MSG_SUCCESS);
    msg_envelope_elem_body_t* c = msgbus_msg_envelope_new_integer(1);
    ASSERT_NOT_NULL(c);
    ret = msgbus_msg_envelope_put(env, "channels", c);
    ASSERT_EQ(ret, MSG_SUCCESS);

    // Add additional frame meta-data
    msg_envelope_elem_body_t* add = msgbus_msg_envelope_new_string("test");
    ASSERT_NOT_NULL(add);
    ret = msgbus_msg_envelope_put(env, "ADDED", add);
    ASSERT_EQ(ret, MSG_SUCCESS);

    // Add data blob (i.e. raw frame data)
    char* blob_data = (char*) malloc(sizeof(char) * 14);
    memcpy(blob_data, "Hello, World!", 14);
    blob_data[13] = '\0';
    msg_envelope_elem_body_t* b = msgbus_msg_envelope_new_blob(blob_data, 14);
    ASSERT_NOT_NULL(b);
    ret = msgbus_msg_envelope_put(env, NULL, b);
    ASSERT_EQ(ret, MSG_SUCCESS);

    // Deserialize the frame
    Frame* frame = new Frame(env);

    // Verify common meta-data
    ASSERT_EQ(frame->get_width(), 14);
    ASSERT_EQ(frame->get_height(), 1);
    ASSERT_EQ(frame->get_channels(), 1);

    // Verify data
    char* frame_data = (char*) frame->get_data(0);
    ASSERT_EQ(strcmp(frame_data, "Hello, World!"), 0);

    // Verify additional meta-data
    msg_envelope_t* meta = frame->get_meta_data();

    msg_envelope_elem_body_t* a;
    ret = msgbus_msg_envelope_get(meta, "ADDED", &a);
    ASSERT_EQ(ret, MSG_SUCCESS);
    ASSERT_EQ(a->type, MSG_ENV_DT_STRING);
    ASSERT_EQ(strcmp(a->body.string, "test"), 0);

    delete frame;
}

TEST_F(frame_tests, deserialize_reserialize) {
    // Create initial message envelope (represents a message received over the
    // message bus)
    msg_envelope_t* env = msgbus_msg_envelope_new(CT_JSON);
    ASSERT_NOT_NULL(env);

    // Add basic meta-data
    msg_envelope_elem_body_t* w = msgbus_msg_envelope_new_integer(14);
    ASSERT_NOT_NULL(w);
    msgbus_ret_t ret = msgbus_msg_envelope_put(env, "width", w);
    ASSERT_EQ(ret, MSG_SUCCESS);
    msg_envelope_elem_body_t* h = msgbus_msg_envelope_new_integer(1);
    ASSERT_NOT_NULL(h);
    ret = msgbus_msg_envelope_put(env, "height", h);
    ASSERT_EQ(ret, MSG_SUCCESS);
    msg_envelope_elem_body_t* c = msgbus_msg_envelope_new_integer(1);
    ASSERT_NOT_NULL(c);
    ret = msgbus_msg_envelope_put(env, "channels", c);
    ASSERT_EQ(ret, MSG_SUCCESS);

    // Add data blob (i.e. raw frame data)
    char* blob_data = (char*) malloc(sizeof(char) * 14);
    memcpy(blob_data, "Hello, World!", 14);
    blob_data[13] = '\0';
    msg_envelope_elem_body_t* b = msgbus_msg_envelope_new_blob(blob_data, 14);
    ASSERT_NOT_NULL(b);
    ret = msgbus_msg_envelope_put(env, NULL, b);
    ASSERT_EQ(ret, MSG_SUCCESS);

    // Deserialize the frame
    Frame* frame = new Frame(env);

    // Verify common meta-data
    ASSERT_EQ(frame->get_width(), 14);
    ASSERT_EQ(frame->get_height(), 1);
    ASSERT_EQ(frame->get_channels(), 1);

    // Verify data
    char* frame_data = (char*) frame->get_data(0);
    ASSERT_EQ(strcmp(frame_data, "Hello, World!"), 0);

    // Add additional meta-data
    msg_envelope_t* meta = frame->get_meta_data();

    // Add additional frame meta-data
    msg_envelope_elem_body_t* add = msgbus_msg_envelope_new_string("test");
    ASSERT_NOT_NULL(add);
    ret = msgbus_msg_envelope_put(meta, "ADDED", add);
    ASSERT_EQ(ret, MSG_SUCCESS);

    // Modifying the data
    char* data = (char*) frame->get_data(0);
    memcpy(data, "Goodbye", 8);
    data[7] = '\0';

    // Serialize the frame
    msg_envelope_t* s = frame->serialize();

    // Verify that the methods for the frame return NULL after serialization
    ASSERT_NULL(frame->get_meta_data());
    ASSERT_NULL(frame->serialize());
    ASSERT_NULL(frame->get_data(0));

    // Get the additional meta-data and make sure it exists
    msg_envelope_elem_body_t* a;
    ret = msgbus_msg_envelope_get(s, "ADDED", &a);
    ASSERT_EQ(ret, MSG_SUCCESS);
    ASSERT_EQ(a->type, MSG_ENV_DT_STRING);
    ASSERT_EQ(strcmp(a->body.string, "test"), 0);

    // Verify the blob is the correct data
    msg_envelope_elem_body_t* blob;
    ret = msgbus_msg_envelope_get(s, NULL, &blob);
    ASSERT_EQ(ret, MSG_SUCCESS);
    ASSERT_EQ(blob->type, MSG_ENV_DT_BLOB);
    ASSERT_EQ(strcmp(blob->body.blob->data, "Goodbye"), 0);

    // Destroy the serialized message, this should ultimately destroy
    // everything
    msgbus_msg_envelope_destroy(s);
    delete frame;
}

// Free method for OpenCV read in frame, does nothing
void free_frame(void*) {}

// Basic sanity test for initializing a and deleting a multi frame before
// serialization
TEST_F(frame_tests, multi_frame_basic_init) {
    Frame* frame = init_multi_frame();

    ASSERT_EQ(frame->get_width(), 14);
    ASSERT_EQ(frame->get_height(), 1);
    ASSERT_EQ(frame->get_channels(), 1);

    ASSERT_EQ(frame->get_width(1), 14);
    ASSERT_EQ(frame->get_height(1), 1);
    ASSERT_EQ(frame->get_channels(1), 1);
    ASSERT_EQ(frame->get_encode_level(1), 0);
    ASSERT_EQ(frame->get_encode_type(1), EncodeType::NONE);

    delete frame;
}

// Basic test for serializing a frame and verifying the meta-data and blob of
// the serialized frame
TEST_F(frame_tests, multi_frame_basic_serialize_free) {
    Frame* frame = init_multi_frame();
    msg_envelope_t* msg = frame->serialize();

    // Verify that the methods for the frame return NULL after serialization
    ASSERT_NULL(frame->get_meta_data());
    ASSERT_NULL(frame->serialize());
    ASSERT_NULL(frame->get_data(0));

    // Verify the meta-data exists
    msg_envelope_elem_body_t* w;
    msgbus_ret_t ret = msgbus_msg_envelope_get(msg, "width", &w);
    ASSERT_EQ(ret, MSG_SUCCESS);
    ASSERT_EQ(w->type, MSG_ENV_DT_INT);
    ASSERT_EQ(w->body.integer, 14);

    msg_envelope_elem_body_t* h;
    ret = msgbus_msg_envelope_get(msg, "height", &h);
    ASSERT_EQ(ret, MSG_SUCCESS);
    ASSERT_EQ(h->type, MSG_ENV_DT_INT);
    ASSERT_EQ(h->body.integer, 1);

    msg_envelope_elem_body_t* c;
    ret = msgbus_msg_envelope_get(msg, "channels", &c);
    ASSERT_EQ(ret, MSG_SUCCESS);
    ASSERT_EQ(c->type, MSG_ENV_DT_INT);
    ASSERT_EQ(c->body.integer, 1);

    // Verify the blob is the correct data
    msg_envelope_elem_body_t* blob;
    ret = msgbus_msg_envelope_get(msg, NULL, &blob);
    ASSERT_EQ(ret, MSG_SUCCESS);
    ASSERT_EQ(blob->type, MSG_ENV_DT_ARRAY);
    for (int i = 0; i < blob->body.array->len; i++) {
        msg_envelope_elem_body_t* arr_blob = \
                    msgbus_msg_envelope_elem_array_get_at(blob, i);
        if (i == 0) {
            ASSERT_EQ(strcmp(arr_blob->body.blob->data, "Hello, World1"), 0);
        }
        if (i == 1) {
            ASSERT_EQ(strcmp(arr_blob->body.blob->data, "Hello, World2"), 0);
        }
    }

    msgbus_msg_envelope_destroy(msg);
    delete frame;
}

// Test that the modification of the data works correctly
TEST_F(frame_tests, multi_frame_modify_data) {
    Frame* frame = init_multi_frame();

    // Modifying the data
    char* data = (char*) frame->get_data(0);
    memcpy(data, "Goodbye", 8);
    data[7] = '\0';

    // Add meta-data
    msg_envelope_t* meta = frame->get_meta_data();

    msg_envelope_elem_body_t* add = msgbus_msg_envelope_new_string("test");
    ASSERT_NOT_NULL(add);

    msgbus_ret_t ret = msgbus_msg_envelope_put(meta, "ADDED", add);
    ASSERT_EQ(ret, MSG_SUCCESS);

    // Serialize and verify the message
    msg_envelope_t* msg = frame->serialize();

    msg_envelope_elem_body_t* a;
    ret = msgbus_msg_envelope_get(msg, "ADDED", &a);
    ASSERT_EQ(ret, MSG_SUCCESS);
    ASSERT_EQ(a->type, MSG_ENV_DT_STRING);
    ASSERT_EQ(strcmp(a->body.string, "test"), 0);

    // Verify the blob is the correct data
    msg_envelope_elem_body_t* blob;
    ret = msgbus_msg_envelope_get(msg, NULL, &blob);
    ASSERT_EQ(ret, MSG_SUCCESS);
    ASSERT_EQ(blob->type, MSG_ENV_DT_ARRAY);
    for (int i = 0; i < blob->body.array->len; i++) {
        msg_envelope_elem_body_t* arr_blob = \
                    msgbus_msg_envelope_elem_array_get_at(blob, i);
        if (i == 0) {
            ASSERT_EQ(strcmp(arr_blob->body.blob->data, "Goodbye"), 0);
        }
        if (i == 1) {
            ASSERT_EQ(strcmp(arr_blob->body.blob->data, "Hello, World2"), 0);
        }
    }

    msgbus_msg_envelope_destroy(msg);
    delete frame;
}

// Basic deserialization test
TEST_F(frame_tests, multi_frame_basic_deserialize) {
    // Create initial message envelope (represents a message received over the
    // message bus)
    msg_envelope_t* env = msgbus_msg_envelope_new(CT_JSON);
    ASSERT_NOT_NULL(env);

    // Add basic meta-data
    msg_envelope_elem_body_t* handle = msgbus_msg_envelope_new_string(
            "img-handle-test");
    ASSERT_NOT_NULL(handle);
    msgbus_ret_t ret = msgbus_msg_envelope_put(env, "img_handle", handle);
    ASSERT_EQ(ret, MSG_SUCCESS);
    msg_envelope_elem_body_t* w = msgbus_msg_envelope_new_integer(14);
    ASSERT_NOT_NULL(w);
    ret = msgbus_msg_envelope_put(env, "width", w);
    ASSERT_EQ(ret, MSG_SUCCESS);
    msg_envelope_elem_body_t* h = msgbus_msg_envelope_new_integer(1);
    ASSERT_NOT_NULL(h);
    ret = msgbus_msg_envelope_put(env, "height", h);
    ASSERT_EQ(ret, MSG_SUCCESS);
    msg_envelope_elem_body_t* c = msgbus_msg_envelope_new_integer(1);
    ASSERT_NOT_NULL(c);
    ret = msgbus_msg_envelope_put(env, "channels", c);
    ASSERT_EQ(ret, MSG_SUCCESS);

    // Add additional frame meta-data
    msg_envelope_elem_body_t* add = msgbus_msg_envelope_new_string("test");
    ASSERT_NOT_NULL(add);
    ret = msgbus_msg_envelope_put(env, "ADDED", add);
    ASSERT_EQ(ret, MSG_SUCCESS);

    // Add data blob (i.e. raw frame data)
    char* blob_data = (char*) malloc(sizeof(char) * 14);
    memcpy(blob_data, "Hello, World1", 14);
    blob_data[13] = '\0';
    msg_envelope_elem_body_t* b = msgbus_msg_envelope_new_blob(blob_data, 14);
    ASSERT_NOT_NULL(b);
    ret = msgbus_msg_envelope_put(env, NULL, b);
    ASSERT_EQ(ret, MSG_SUCCESS);

    char* blob_data_two = (char*) malloc(sizeof(char) * 14);
    memcpy(blob_data_two, "Hello, World2", 14);
    blob_data_two[13] = '\0';
    msg_envelope_elem_body_t* b_2 = msgbus_msg_envelope_new_blob(
            blob_data_two, 14);
    ASSERT_NOT_NULL(b_2);
    ret = msgbus_msg_envelope_put(env, NULL, b_2);
    ASSERT_EQ(ret, MSG_SUCCESS);

    // Add additional frames meta-data
    msg_envelope_elem_body_t* obj = msgbus_msg_envelope_new_object();
    msg_envelope_elem_body_t* handle2 = msgbus_msg_envelope_new_string(
            "img-handle2-test");
    ASSERT_NOT_NULL(handle2);
    ret = msgbus_msg_envelope_elem_object_put(obj, "img_handle", handle2);
    ASSERT_EQ(ret, MSG_SUCCESS);
    msg_envelope_elem_body_t* w2 = msgbus_msg_envelope_new_integer(14);
    ASSERT_NOT_NULL(w2);
    ret = msgbus_msg_envelope_elem_object_put(obj, "width", w2);
    ASSERT_EQ(ret, MSG_SUCCESS);
    msg_envelope_elem_body_t* h2 = msgbus_msg_envelope_new_integer(1);
    ASSERT_NOT_NULL(h2);
    ret = msgbus_msg_envelope_elem_object_put(obj, "height", h2);
    ASSERT_EQ(ret, MSG_SUCCESS);
    msg_envelope_elem_body_t* c2 = msgbus_msg_envelope_new_integer(1);
    ASSERT_NOT_NULL(c2);
    ret = msgbus_msg_envelope_elem_object_put(obj, "channels", c2);
    ASSERT_EQ(ret, MSG_SUCCESS);

    msg_envelope_elem_body_t* arr = msgbus_msg_envelope_new_array();
    ASSERT_NOT_NULL(arr);
    ret = msgbus_msg_envelope_elem_array_add(arr, obj);
    ASSERT_EQ(ret, MSG_SUCCESS);

    ret = msgbus_msg_envelope_put(env, "additional_frames", arr);
    ASSERT_EQ(ret, MSG_SUCCESS);

    // Deserialize the frame
    Frame* frame = new Frame(env);

    // Verify common meta-data
    ASSERT_EQ(frame->get_width(), 14);
    ASSERT_EQ(frame->get_height(), 1);
    ASSERT_EQ(frame->get_channels(), 1);
    EXPECT_TRUE(frame->get_img_handle() == "img-handle-test");

    ASSERT_EQ(frame->get_width(1), 14);
    ASSERT_EQ(frame->get_height(1), 1);
    ASSERT_EQ(frame->get_channels(1), 1);
    EXPECT_TRUE(frame->get_img_handle(1) == "img-handle2-test");

    // Verify data
    char* frame_data = (char*) frame->get_data(0);
    ASSERT_EQ(strcmp(frame_data, "Hello, World1"), 0);

    char* frame_data_two = (char*) frame->get_data(1);
    ASSERT_EQ(strcmp(frame_data_two, "Hello, World2"), 0);

    // Verify additional meta-data
    msg_envelope_t* meta = frame->get_meta_data();

    msg_envelope_elem_body_t* a;
    ret = msgbus_msg_envelope_get(meta, "ADDED", &a);
    ASSERT_EQ(ret, MSG_SUCCESS);
    ASSERT_EQ(a->type, MSG_ENV_DT_STRING);
    ASSERT_EQ(strcmp(a->body.string, "test"), 0);

    delete frame;
}

TEST_F(frame_tests, multi_frame_deserialize_reserialize) {
    // Create initial message envelope (represents a message received over the
    // message bus)
    msg_envelope_t* env = msgbus_msg_envelope_new(CT_JSON);
    ASSERT_NOT_NULL(env);

    // Add basic meta-data
    msg_envelope_elem_body_t* handle = msgbus_msg_envelope_new_string(
            "img-handle-test");
    ASSERT_NOT_NULL(handle);
    msgbus_ret_t ret = msgbus_msg_envelope_put(env, "img_handle", handle);
    ASSERT_EQ(ret, MSG_SUCCESS);
    msg_envelope_elem_body_t* w = msgbus_msg_envelope_new_integer(14);
    ASSERT_NOT_NULL(w);
    ret = msgbus_msg_envelope_put(env, "width", w);
    ASSERT_EQ(ret, MSG_SUCCESS);
    msg_envelope_elem_body_t* h = msgbus_msg_envelope_new_integer(1);
    ASSERT_NOT_NULL(h);
    ret = msgbus_msg_envelope_put(env, "height", h);
    ASSERT_EQ(ret, MSG_SUCCESS);
    msg_envelope_elem_body_t* c = msgbus_msg_envelope_new_integer(1);
    ASSERT_NOT_NULL(c);
    ret = msgbus_msg_envelope_put(env, "channels", c);
    ASSERT_EQ(ret, MSG_SUCCESS);

    // Add data blob (i.e. raw frame data)
    char* blob_data = (char*) malloc(sizeof(char) * 14);
    memcpy(blob_data, "Hello, World1", 14);
    blob_data[13] = '\0';
    msg_envelope_elem_body_t* b = msgbus_msg_envelope_new_blob(blob_data, 14);
    ASSERT_NOT_NULL(b);
    ret = msgbus_msg_envelope_put(env, NULL, b);
    ASSERT_EQ(ret, MSG_SUCCESS);

    char* blob_data_two = (char*) malloc(sizeof(char) * 14);
    memcpy(blob_data_two, "Hello, World2", 14);
    blob_data_two[13] = '\0';
    msg_envelope_elem_body_t* b_2 = msgbus_msg_envelope_new_blob(
            blob_data_two, 14);
    ASSERT_NOT_NULL(b_2);
    ret = msgbus_msg_envelope_put(env, NULL, b_2);
    ASSERT_EQ(ret, MSG_SUCCESS);

    // Add additional frames meta-data
    msg_envelope_elem_body_t* obj = msgbus_msg_envelope_new_object();
    msg_envelope_elem_body_t* handle2 = msgbus_msg_envelope_new_string(
            "img-handle2-test");
    ASSERT_NOT_NULL(handle2);
    ret = msgbus_msg_envelope_elem_object_put(obj, "img_handle", handle2);
    ASSERT_EQ(ret, MSG_SUCCESS);
    msg_envelope_elem_body_t* w2 = msgbus_msg_envelope_new_integer(14);
    ASSERT_NOT_NULL(w2);
    ret = msgbus_msg_envelope_elem_object_put(obj, "width", w2);
    ASSERT_EQ(ret, MSG_SUCCESS);
    msg_envelope_elem_body_t* h2 = msgbus_msg_envelope_new_integer(1);
    ASSERT_NOT_NULL(h2);
    ret = msgbus_msg_envelope_elem_object_put(obj, "height", h2);
    ASSERT_EQ(ret, MSG_SUCCESS);
    msg_envelope_elem_body_t* c2 = msgbus_msg_envelope_new_integer(1);
    ASSERT_NOT_NULL(c2);
    ret = msgbus_msg_envelope_elem_object_put(obj, "channels", c2);
    ASSERT_EQ(ret, MSG_SUCCESS);

    msg_envelope_elem_body_t* arr = msgbus_msg_envelope_new_array();
    ASSERT_NOT_NULL(arr);
    ret = msgbus_msg_envelope_elem_array_add(arr, obj);
    ASSERT_EQ(ret, MSG_SUCCESS);

    ret = msgbus_msg_envelope_put(env, "additional_frames", arr);
    ASSERT_EQ(ret, MSG_SUCCESS);

    // Deserialize the frame
    Frame* frame = new Frame(env);
    ASSERT_NULL(env->blob);

    // Verify common meta-data
    ASSERT_EQ(frame->get_width(), 14);
    ASSERT_EQ(frame->get_height(), 1);
    ASSERT_EQ(frame->get_channels(), 1);
    EXPECT_TRUE(frame->get_img_handle() == "img-handle-test");

    ASSERT_EQ(frame->get_width(1), 14);
    ASSERT_EQ(frame->get_height(1), 1);
    ASSERT_EQ(frame->get_channels(1), 1);
    EXPECT_TRUE(frame->get_img_handle(1) == "img-handle2-test");

    // Verify data
    char* frame_data = (char*) frame->get_data(0);
    ASSERT_EQ(strcmp(frame_data, "Hello, World1"), 0);

    char* frame_data_two = (char*) frame->get_data(1);
    ASSERT_EQ(strcmp(frame_data_two, "Hello, World2"), 0);

    // Add additional meta-data
    msg_envelope_t* meta = frame->get_meta_data();

    // Add additional frame meta-data
    msg_envelope_elem_body_t* add = msgbus_msg_envelope_new_string("test");
    ASSERT_NOT_NULL(add);
    ret = msgbus_msg_envelope_put(meta, "ADDED", add);
    ASSERT_EQ(ret, MSG_SUCCESS);

    // Modifying the data
    char* data = new char[8];
    memcpy(data, "Goodbye", 8);
    data[7] = '\0';
    frame->set_data(0, (void*) data, free, (void*) data, 8, 1, 1);

    ASSERT_EQ(frame->get_width(), 8);
    ASSERT_EQ(frame->get_height(), 1);
    ASSERT_EQ(frame->get_channels(), 1);
    EXPECT_TRUE(frame->get_img_handle(1) == "img-handle2-test");

    // Serialize the frame
    msg_envelope_t* s = frame->serialize();

    // Verify that the methods for the frame return NULL after serialization
    ASSERT_NULL(frame->get_meta_data());
    ASSERT_NULL(frame->serialize());
    ASSERT_NULL(frame->get_data(0));

    // Get the additional meta-data and make sure it exists
    msg_envelope_elem_body_t* a;
    ret = msgbus_msg_envelope_get(s, "ADDED", &a);
    ASSERT_EQ(ret, MSG_SUCCESS);
    ASSERT_EQ(a->type, MSG_ENV_DT_STRING);
    ASSERT_EQ(strcmp(a->body.string, "test"), 0);

    // Verify the blob is the correct data
    msg_envelope_elem_body_t* blob;
    ret = msgbus_msg_envelope_get(s, NULL, &blob);
    ASSERT_EQ(ret, MSG_SUCCESS);

    ASSERT_EQ(blob->type, MSG_ENV_DT_ARRAY);
    for (int i = 0; i < blob->body.array->len; i++) {
        msg_envelope_elem_body_t* arr_blob = \
                    msgbus_msg_envelope_elem_array_get_at(blob, i);
        printf("blob data %d %s\n", i, arr_blob->body.blob->data);
        if (i == 0) {
            ASSERT_EQ(strcmp(arr_blob->body.blob->data, "Goodbye"), 0);
        }
        if (i == 1) {
            ASSERT_EQ(strcmp(arr_blob->body.blob->data, "Hello, World2"), 0);
        }
    }

    // Destroy the serialized message, this should ultimately destroy
    // everything
    msgbus_msg_envelope_destroy(s);
    delete frame;
}

/**
 * Test to verify the encode/decode flow for a PNG. Note this is just a
 * smoke test, does not guarentee that the bytes are encoded or decoded
 * correctly.
 */
TEST_F(frame_tests, encode_decode_png) {
    // Read in the test frame
    cv::Mat mat_frame = cv::imread("./test_image.png");

    // Initialize the frame objectj
    Frame* frame = new Frame(
            (void*) &mat_frame, free_frame, (void*) mat_frame.data,
            mat_frame.cols, mat_frame.rows, mat_frame.channels(),
            EncodeType::PNG, 4);

    LOG_DEBUG_0("After frame creation");

    // Serialize the frame
    msg_envelope_t* encoded = frame->serialize();

    LOG_DEBUG_0("After frame serialize");

    // Deserialize the frame
    Frame* decoded = new Frame(encoded);

    LOG_DEBUG_0("After frame Deserialize");

    // Write frame for visual inspection of decoding
    cv::Mat mat_decoded_frame(
            decoded->get_height(), decoded->get_width(),
            CV_8UC(decoded->get_channels()), decoded->get_data(0));
    cv::imwrite("frame_tests_encode_decode_png.png", mat_decoded_frame);

    LOG_DEBUG_0("End of the loine");

    // Clean up
    delete decoded;
    delete frame;
}

/**
 * Test to verify the encode/decode flow for a JPEG. Note this is just a
 * smoke test, does not guarentee that the bytes are encoded or decoded
 * correctly.
 */
TEST_F(frame_tests, encode_decode_jpeg) {
    // Read in the test frame
    cv::Mat mat_frame = cv::imread("./test_image.png");

    // Initialize the frame object
    Frame* frame = new Frame(
            (void*) &mat_frame, free_frame, (void*) mat_frame.data,
            mat_frame.cols, mat_frame.rows, mat_frame.channels(),
            EncodeType::JPEG, 50);

    // Serialize the frame
    msg_envelope_t* encoded = frame->serialize();

    // Deserialize the frame
    Frame* decoded = new Frame(encoded);

    // Write frame for visual inspection of decoding
    cv::Mat mat_decoded_frame(
            decoded->get_height(), decoded->get_width(),
            CV_8UC(decoded->get_channels()), decoded->get_data(0));
    cv::imwrite("frame_tests_encode_decode_jpeg.jpeg", mat_decoded_frame);

    // Re-encode
    msg_envelope_t* serialized = decoded->serialize();

    // Clean up
    // delete decoded;
    msgbus_msg_envelope_destroy(serialized);
    delete frame;
    delete decoded;
}

/**
 * Helper for a base unit test to call set_data() on a specific index. This
 * also allows for chnaging the encoding type to make sure this works across
 * all enc types.
 */
void base_set_data_test(int index, EncodeType enc_type, int enc_lvl) {
    try {
        const char* fn = "./test_image.png";
        Frame* frame = new Frame();

        for (int i = 0; i < 2; i++) {
            // Read multiple frames
            cv::Mat* cv_frame = new cv::Mat();
            *cv_frame = cv::imread(fn);
            frame->add_frame(
                (void*) cv_frame, free_cv_frame, (void*) cv_frame->data,
                cv_frame->cols, cv_frame->rows, cv_frame->channels(),
                enc_type, enc_lvl);
        }

        // Call set_data() to change frame 1
        cv::Mat* cv_frame = new cv::Mat();
        *cv_frame = cv::imread(fn);

        LOG_DEBUG_0("Setting data");
        frame->set_data(
            index, (void*) cv_frame, free_cv_frame, (void*) cv_frame->data,
            cv_frame->cols, cv_frame->rows, cv_frame->channels());

        // Serialize the frame
        LOG_DEBUG_0("Serializing frame");
        msg_envelope_t* encoded = frame->serialize();

        // Deserialize the frame
        LOG_DEBUG_0("Deserializing serialized frame");
        Frame* decoded = new Frame(encoded);

        // Re-encode
        LOG_DEBUG_0("Reserializing again");
        msg_envelope_t* serialized = decoded->serialize();

        // Clean up
        // delete decoded;
        LOG_DEBUG_0("Destroying serialized frame");
        msgbus_msg_envelope_destroy(serialized);
        delete decoded;
        delete frame;
    } catch (const char* ex) {
        LOG_ERROR("Test failed: %s", ex);
        throw ex;
    }
}

/**
 * Attempt @c Frame::set_data() with non-zero index.
 */
TEST_F(frame_tests, multi_frame_set_data_1_jpeg) {
    base_set_data_test(1, EncodeType::JPEG, 50);
}

TEST_F(frame_tests, multi_frame_set_data_1_png) {
    base_set_data_test(1, EncodeType::PNG, 4);
}

TEST_F(frame_tests, multi_frame_set_data_1) {
    base_set_data_test(1, EncodeType::NONE, 0);
}

TEST_F(frame_tests, multi_frame_set_data_0_jpeg) {
    base_set_data_test(0, EncodeType::JPEG, 50);
}

TEST_F(frame_tests, multi_frame_set_data_0_png) {
    base_set_data_test(0, EncodeType::PNG, 4);
}

TEST_F(frame_tests, multi_frame_set_data_0) {
    base_set_data_test(0, EncodeType::NONE, 0);
}
