// Copyright (c) 2020 Intel Corporation.
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
 */
#ifndef _EII_UDF_UDF_MANAGER_H
#define _EII_UDF_UDF_MANAGER_H

#include <thread>
#include <atomic>
#include <vector>
#include <eii/utils/config.h>
#include <eii/utils/thread_safe_queue.h>
#include <eii/utils/thread_executor.hpp>
#include <eii/utils/profiling.h>

#include "eii/udf/udf_handle.h"
#include "eii/udf/frame.h"

namespace eii {
namespace udf {

typedef utils::ThreadSafeQueue<Frame*> FrameQueue;

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

    // Thread executor
    utils::ThreadExecutor* m_executor;

    // UDF Handles
    std::vector<UdfHandle*> m_udfs;

    // Profiling handle
    utils::Profiling* m_profile;

    // Queue blocked variable
    std::string m_udf_push_block_key;

    // UDF exit profiling key
    std::string m_udf_push_entry_key;

    // Caller's AppName
    std::string m_service_name;

    // Encoding details
    EncodeType m_enc_type;
    int m_enc_lvl;

    /**
     * @c UDFManager private thread run method.
     */
    void run(int tid, std::atomic<bool>& stop, void* varg);

    /**
     * Private @c UdfManager copy constructor.
     */
    UdfManager(const UdfManager& src);

    /**
     * Private @c UdfManager assignment operator.
     */
    UdfManager& operator=(const UdfManager& src);

public:
    /**
     * Constructor
     *
     * @param udf_cfg      - UDF configurations
     * @param input_queue  - Input frame queue
     * @param output_queue - Output frame queue
     * @param enc_type     - Encoding to use on all frames put into the output
     *                       queue. (df: EncodeType::NONE)
     * @param enc_lvl      - Encoding level, must be between 0 and 9 for PNG
     *                       and 0 and 100 for JPEG (df: 0)
     */
    UdfManager(config_t* udf_cfg, FrameQueue* input_queue,
               FrameQueue* output_queue, std::string service_name,
               EncodeType enc_type=EncodeType::NONE,
               int enc_lvl=0);

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
} // eii
#endif // _EII_UDF_UDF_MANAGER_H
