// Copyright (c) 2019 Intel Corporation.  //
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
 * @brief UDF loading example
 * @author Kevin Midkiff <kevin.midkiff@intel.com>
 */

#include <cstring>
#include <thread>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <eis/utils/logger.h>
#include <eis/utils/json_config.h>
#include <eis/msgbus/msgbus.h>
#include <opencv2/opencv.hpp>
#include "eis/udf/udf_manager.h"

#define ORIG_FRAME_DATA "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
#define DATA_LEN 10

using namespace eis::udf;
using namespace eis::utils;
using namespace eis::msgbus;

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

void free_cv_frame(void* frame) {
}

int main(int argc, char** argv) {
    try {
        set_log_level(LOG_LVL_DEBUG);
        config_t* config = json_config_new("config.json");
        config_t* msgbus_config = json_config_new("msgbus_config.json");

        FrameQueue* input_queue = new FrameQueue(-1);
        FrameQueue* output_queue = new FrameQueue(-1);

        LOG_INFO_0("Initializing UDFManager");
        UdfManager* manager = new UdfManager(config, input_queue, output_queue);
        manager->start();

        LOG_INFO_0("Initializing Publisher thread");
        std::condition_variable err_cv;
        Publisher* publisher = new Publisher(
                msgbus_config, err_cv, "example", (MessageQueue*) output_queue);
        publisher->start();

        LOG_INFO_0("Adding frames to input queue");
        cv::Mat cv_frame = cv::imread("0.png");
        Frame* frame = new Frame(
                (void*) &cv_frame, cv_frame.cols, cv_frame.rows,
                cv_frame.channels(), cv_frame.data, free_cv_frame);
        //input_queue->push(frame);
        msg_envelope_t* msg = frame->serialize();
        Frame* deserial_frame = new Frame(msg);
        input_queue->push(deserial_frame);

        // for(int i = 0; i < 30; i++) {
        //     input_queue->push(init_frame());
        // }

        LOG_INFO_0("Waiting for input queue to be empty");
        // while(!input_queue->empty() && !output_queue->empty()) {}
        std::this_thread::sleep_for(std::chrono::seconds(3));

        LOG_INFO_0("Stopping the publisher");
        publisher->stop();

        LOG_INFO_0("Stopping the UDFManager");
        manager->stop();

        LOG_INFO_0("Cleaning up publisher");
        delete publisher;

        LOG_INFO_0("Cleaning up UDFManager");
        delete manager;
    } catch(const char* ex) {
        LOG_INFO("Failed to load exception: %s", ex);
        return -1;
    }

    return 0;
}
