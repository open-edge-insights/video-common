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
 * @file
 * @brief UDF Manager thread with input/output frame queues.
 * @author Kevin Midkiff (kevin.midkiff@intel.com)
 */
#ifndef _EIS_UDF_UDF_MANAGER_H
#define _EIS_UDF_UDF_MANAGER_H

#include <thread>
#include <atomic>
#include <vector>
#include <eis/utils/frame.h>
#include <eis/utils/config.h>
#include <eis/utils/thread_safe_queue.h>
#include <eis/utils/thread_pool.h>

#include "eis/udf/udf_handle.h"
#include "eis/udf/loader.h"

namespace eis {
namespace udf {

typedef utils::ThreadSafeQueue<utils::Frame*> FrameQueue;

/**
 * UdfManager class
 */
class UdfManager {
private:
    // Thread
    std::thread* m_th;

    // Flag to stop the UDFManager thread
    std::atomic<bool> m_stop;

    // Configuration
    config_t* m_config;

    // UDF input queue
    FrameQueue* m_udf_input_queue;

    // UDF output queue
    FrameQueue* m_udf_output_queue;

    // UDF Loader
    UdfLoader* m_loader;

    // Thread pool
    utils::ThreadPool* m_pool;

    // UDF Handles
    std::vector<UdfHandle*> m_udfs;

    /**
     * @c UDFManager private thread run method.
     */
    void run();

public:
    /**
     * Constructor
     *
     * @param udf_cfg      - UDF configurations
     * @param input_queue  - Input frame queue
     * @param output_queue - Output frame queue
     */
    UdfManager(config_t* udf_cfg, FrameQueue* input_queue,
               FrameQueue* output_queue);

    /**
     * Destructor
     */
    ~UdfManager();

    /**
     * Start the UDFManager thread
     */
    void start();

    /**
     * Stop the UDFManager thread
     */
    void stop();
};

} // udf
} // eis
#endif // _EIS_UDF_UDF_MANAGER_H
