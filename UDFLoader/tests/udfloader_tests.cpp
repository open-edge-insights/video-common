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
 * @brief Unit tests for the @c UDFLoader object and for calling UDFs
 */

#include <chrono>
#include <cassert>
#include <gtest/gtest.h>
#include <opencv2/opencv.hpp>
#include <eii/utils/logger.h>
#include <eii/utils/json_config.h>
#include <eii/utils/string.h>
#include "eii/udf/loader.h"
#include "eii/udf/udf_manager.h"

#define LD_PATH_SET     "LD_LIBRARY_PATH="
#define LD_SEP          ":"
#define ORIG_FRAME_DATA "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
#define NEW_FRAME_DATA  "\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01"
#define DATA_LEN 10

using namespace eii::udf;

#define ASSERT_NULL(val) { \
    if(val != NULL) FAIL() << "Value should be NULL"; \
}

#define ASSERT_NOT_NULL(val) { \
    if(val == NULL) FAIL() << "Value shoud not be NULL"; \
}

// Prototypes
static char* update_ld_library_path();

//
// Helper objects
//

/**
 * Test object to represent a video frame
 */
class TestFrame {
public:
    uint8_t* data;

    TestFrame(uint8_t* data) : data(data)
    {};

    ~TestFrame() { delete[] data; };
};

void test_frame_free(void* hint) {
    TestFrame* tf = (TestFrame*) hint;
    delete tf;
}

Frame* init_frame() {
    uint8_t* data = new uint8_t[DATA_LEN];
    memcpy(data, ORIG_FRAME_DATA, DATA_LEN);

    TestFrame* tf = new TestFrame(data);

    Frame* frame = new Frame(
            (void*) tf, DATA_LEN, 1, 1, (void*) data, test_frame_free);

    return frame;
}

UdfLoader* loader = NULL;

// Test to modify the underlying frame from a Python UDF and to modify the meta
// data. This test also tests the UDFLoader's ability to load a UDF that is in
// a Python package.
TEST(udfloader_tests, py_modify) {
    // Load a configuration
    config_t* config = json_config_new("test_config.json");
    ASSERT_NOT_NULL(config);

    // Initialize the UDFLoader and load the UDF
    UdfHandle* handle = loader->load("py_tests.modify", config, 1);
    ASSERT_NOT_NULL(handle);

    // Initialize the frame to use
    Frame* frame = init_frame();
    ASSERT_NOT_NULL(frame);

    // Execute the UDF over the frame
    UdfRetCode ret = handle->process(frame);
    ASSERT_EQ(ret, UdfRetCode::UDF_OK);

    // Verify frame data is correct
    uint8_t* frame_data = (uint8_t*) frame->get_data();
    for(int i = 0; i < DATA_LEN; i++) {
        ASSERT_EQ(frame_data[i], NEW_FRAME_DATA[i]);
    }

    // Verify that the added meta-data shows up in the meta-data of the frame
    msg_envelope_t* meta = frame->get_meta_data();
    ASSERT_NOT_NULL(meta);

    msg_envelope_elem_body_t* added;
    msgbus_ret_t m_ret = msgbus_msg_envelope_get(meta, "ADDED", &added);
    ASSERT_EQ(m_ret, MSG_SUCCESS);
    ASSERT_EQ(added->type, MSG_ENV_DT_INT);
    ASSERT_EQ(added->body.integer, 55);

    // Clean up
    delete frame;
    delete handle;
}

// Test to drop a frame
TEST(udfloader_tests, py_drop_frame) {
    // Load a configuration
    config_t* config = json_config_new("test_config.json");
    ASSERT_NOT_NULL(config);

    // Initialize the UDFLoader and load the UDF
    UdfHandle* handle = loader->load("py_tests.drop", config, 1);
    ASSERT_NOT_NULL(handle);

    // Initialize the frame to use
    Frame* frame = init_frame();
    ASSERT_NOT_NULL(frame);

    // Execute the UDF over the frame
    UdfRetCode ret = handle->process(frame);
    ASSERT_EQ(ret, UdfRetCode::UDF_DROP_FRAME);

    // Clean up
    delete frame;
    delete handle;
}

// Test to verify configuration
TEST(udfloader_tests, py_config) {
    // Load a configuration
    config_t* config = json_config_new("test_config.json");
    ASSERT_NOT_NULL(config);

    // Initialize the UDFLoader and load the UDF
    UdfHandle* handle = loader->load("py_tests.config", config, 1);
    ASSERT_NOT_NULL(handle);

    // Clean up
    delete handle;
}

// Test for exception in constructor
TEST(udfloader_tests, py_constructor_error) {
    // Load a configuration
    config_t* config = json_config_new("test_config.json");
    ASSERT_NOT_NULL(config);

    // Initialize the UDFLoader and load the UDF
    UdfHandle* handle = loader->load("py_tests.error", config, 1);
    ASSERT_NULL(handle);

    // Clean up
    delete handle;
}

// Test exception in process
TEST(udfloader_tests, py_process_error) {
    // Load a configuration
    config_t* config = json_config_new("test_config.json");
    ASSERT_NOT_NULL(config);

    // Initialize the UDFLoader and load the UDF
    UdfHandle* handle = loader->load("py_tests.process_error", config, 1);
    ASSERT_NOT_NULL(handle);

    // Initialize the frame to use
    Frame* frame = init_frame();
    ASSERT_NOT_NULL(frame);

    // Execute the UDF over the frame
    UdfRetCode ret = handle->process(frame);
    ASSERT_EQ(ret, UdfRetCode::UDF_ERROR);

    // Clean up
    delete frame;
    delete handle;
}

TEST(udfloader_tests, reinitialize) {
    try {
        config_t* config = json_config_new("test_udf_mgr_config.json");
        ASSERT_NOT_NULL(config);

        FrameQueue* input_queue = new FrameQueue(-1);
        FrameQueue* output_queue = new FrameQueue(-1);

        UdfManager* manager = new UdfManager(
                config, input_queue, output_queue, "");
        manager->start();

        Frame* frame = init_frame();
        ASSERT_NOT_NULL(frame);

        input_queue->push(frame);

        std::this_thread::sleep_for(std::chrono::seconds(3));

        delete manager;

        input_queue = new FrameQueue(-1);
        output_queue = new FrameQueue(-1);
        config = json_config_new("test_udf_mgr_config.json");
        manager = new UdfManager(config, input_queue, output_queue, "");
        manager->start();
        std::this_thread::sleep_for(std::chrono::seconds(3));
        delete manager;
    } catch(const std::exception& ex) {
        FAIL() << ex.what();
    }
}

// Free method for OpenCV read in frame, does nothing
static void free_frame(void* varg) {
    cv::Mat* frame = (cv::Mat*) varg;
    delete frame;
}

/**
 * Unit test to load in an actual image, modify it in a Python UDF, and then
 * re-encode the image. This is to make sure the transfer of memory is all
 * taken care of successfully.
 */
TEST(udfloader_tests, modify_frame_encode) {
    try {
        // Read in the test frame
        cv::Mat* mat_frame = new cv::Mat();
        *mat_frame = cv::imread("./test_image.png");
        ASSERT_FALSE(mat_frame->empty()) << "Failed to load test_image.png";

        // Initialize the frame object
        Frame* frame = new Frame(
                (void*) mat_frame, mat_frame->cols, mat_frame->rows,
                mat_frame->channels(), mat_frame->data, free_frame);

        // Load the JSON configuration
        config_t* config = json_config_new("test_udf_mgr_same_frame.json");
        ASSERT_NOT_NULL(config);

        // Initialize the input/output frame queues
        FrameQueue* input_queue = new FrameQueue(-1);
        FrameQueue* output_queue = new FrameQueue(-1);

        // Initialize the UDF manager
        UdfManager* manager = new UdfManager(
                config, input_queue, output_queue, "modify_frame_encode",
                EncodeType::JPEG, 50);
        manager->start();

        // Push the frame into the input queue
        input_queue->push(frame);

        // Wait for the frame in the output queue
        auto sleep_time = std::chrono::seconds(3);
        ASSERT_TRUE(output_queue->wait_for(sleep_time)) << "No frame";

        // Get the frame from the output queue
        Frame* output_frame = output_queue->pop();
        msg_envelope_t* encoded = output_frame->serialize();
        ASSERT_NOT_NULL(encoded);

        // Clean up
        msgbus_msg_envelope_destroy(encoded);
        delete manager;
    } catch(const char* ex) {
        FAIL() << ex;
    }
}

/**
 * Overridden GTest main method
 */
GTEST_API_ int main(int argc, char** argv) {
    // Update the LD_LIBRARY_PATH
    update_ld_library_path();

    // Parse out gTest command line parameters
    ::testing::InitGoogleTest(&argc, argv);

    // Check if log level provided
    if (argc == 3) {
        if (strcmp(argv[1], "--log-level") == 0) {
            // LOG_INFO_0("Running msgbus tests over TCP");
            char* log_level = argv[2];

            if (strcmp(log_level, "INFO") == 0) {
                set_log_level(LOG_LVL_INFO);
            } else if (strcmp(log_level, "DEBUG") == 0) {
                set_log_level(LOG_LVL_DEBUG);
            } else if (strcmp(log_level, "ERROR") == 0) {
                set_log_level(LOG_LVL_ERROR);
            } else if (strcmp(log_level, "WARN") == 0) {
                set_log_level(LOG_LVL_WARN);
            } else {
                LOG_ERROR("Unknown log level: %s", log_level);
                return -1;
            }
        } else {
            LOG_ERROR("Unknown parameter: %s", argv[1]);
            return -1;
        }
    } else if (argc == 2) {
        LOG_ERROR_0("Incorrect number of arguments");
        return -1;
    }

    loader = new UdfLoader();
    int res = RUN_ALL_TESTS();
    delete loader;

    return res;
}

/**
 * Helper method to add the current working directory to the LD_LIBRARY_PATH.
 *
 * Note that this function returns the string for the environmental variable is
 * returned. This memory needs to stay allocated until that variable is no
 * longer needed.
 *
 * @return char*
 */
static char* update_ld_library_path() {
    const char* ld_library_path = getenv("LD_LIBRARY_PATH");
    size_t len = (ld_library_path != NULL) ? strlen(ld_library_path) : 0;

    // Get current working directory
   char cwd[PATH_MAX];
   char* result = getcwd(cwd, PATH_MAX);
   assert(result != NULL);

   size_t dest_len = strlen(LD_PATH_SET) + strlen(cwd) + len + 2;
   char* env_str = NULL;

   if(ld_library_path == NULL) {
       // Setting the environmental variable from scratch
       env_str = concat_s(dest_len, 3, LD_PATH_SET, LD_SEP, cwd);
   } else {
       // Setting the environmental variable with existing path
       env_str = concat_s(
               dest_len, 4, LD_PATH_SET, ld_library_path, LD_SEP, cwd);
   }
   assert(env_str != NULL);

   // Put the new LD_LIBRARY_PATH into the environment
   putenv(env_str);

   return env_str;
}
