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
 * @brief @c UdfManager class implementation.
 * @author Kevin Midkiff (kevin.midkiff@intel.com)
 */

#include <chrono>
#include <eis/utils/logger.h>
#include "eis/udf/udf_manager.h"

using namespace eis::udf;
using namespace eis::utils;

#define CFG_UDFS            "udfs"
#define CFG_MAX_JOBS        "max_jobs"
#define CFG_MAX_WORKERS     "max_workers"
#define DEFAULT_MAX_WORKERS 4 // Default 4 threads to submit jobs to
#define DEFAULT_MAX_JOBS   -1 // Default to having unlimited job queue


void free_fn(void* ptr) {
    config_value_t* obj = (config_value_t*) ptr;
    config_value_destroy(obj);
}

config_value_t* get_config_value(const void* cfg, const char* key) {
    config_value_t* obj = (config_value_t*) cfg;
    return config_value_object_get(obj, key);
}

UdfManager::UdfManager(
        config_t* udf_cfg, FrameQueue* input_queue, FrameQueue* output_queue) :
    m_th(NULL), m_stop(false), m_config(udf_cfg),
    m_udf_input_queue(input_queue), m_udf_output_queue(output_queue),
    m_loader(NULL)
{
    m_loader = new UdfLoader();

    LOG_DEBUG_0("Loading UDFs");
    config_value_t* udfs = config_get(m_config, CFG_UDFS);
    if(udfs == NULL) {
        delete m_loader;
        throw "Failed to get UDFs";
    }
    if(udfs->type != CVT_ARRAY) {
        delete m_loader;
        config_value_destroy(udfs);
        throw "\"udfs\" must be an array";
    }

    // Get maximum jobs (if it exists)
    int max_jobs = DEFAULT_MAX_JOBS;
    config_value_t* cfg_max_jobs = config_get(m_config, CFG_MAX_JOBS);
    if(cfg_max_jobs != NULL) {
        if(cfg_max_jobs->type != CVT_INTEGER) {
            config_value_destroy(cfg_max_jobs);
            config_value_destroy(udfs);
            throw "\"max_jobs\" must be an integer";
        }
        max_jobs = cfg_max_jobs->body.integer;
        config_value_destroy(cfg_max_jobs);
    }

    // Get the maximum number of workers
    int max_workers = DEFAULT_MAX_WORKERS;
    config_value_t* cfg_max_workers = config_get(m_config, CFG_MAX_JOBS);
    if(cfg_max_workers != NULL) {
        if(cfg_max_workers->type != CVT_INTEGER) {
            config_value_destroy(cfg_max_workers);
            config_value_destroy(udfs);
            throw "\"max_jobs\" must be an integer";
        }
        max_workers = cfg_max_jobs->body.integer;
        config_value_destroy(cfg_max_workers);
    }

    LOG_DEBUG("Max workers: %d, max jobs: %d", max_workers, max_jobs);
    // Initialize thread pool
    m_pool = new ThreadPool(max_workers, max_jobs);

    int len = (int) config_value_array_len(udfs);
    for(int i = 0; i < len; i++) {
        config_value_t* cfg_obj = config_value_array_get(udfs, i);
        if(cfg_obj == NULL) {
            throw "Failed to get configuration array element";
        }
        if(cfg_obj->type != CVT_OBJECT) {
            throw "UDF configuration must be objects";
        }
        config_value_t* name = config_value_object_get(cfg_obj, "name");
        if(name == NULL) {
            throw "Failed to get UDF name";
        }
        if(name->type != CVT_STRING) {
            throw "UDF name must be a string";
        }
        // TODO: Add max workers
        void (*free_ptr)(void*) = NULL;
        if(cfg_obj->body.object->free == NULL) {
            free_ptr = free_fn;
        } else {
            free_ptr = cfg_obj->body.object->free;
        }
        config_t* cfg = config_new(
                (void*) cfg_obj, free_ptr, get_config_value);
        if(cfg == NULL) {
            throw "Failed to initialize configuration for UDF";
        }
        LOG_DEBUG("Loading UDF...");
        UdfHandle* handle = m_loader->load(name->body.string, cfg, 1);
        if(handle == NULL) {
            throw "Failed to load UDF";
        }
        config_value_destroy(name);
        m_udfs.push_back(handle);
    }

    config_value_destroy(udfs);
}

UdfManager::~UdfManager() {
    this->stop();
    if(m_th != NULL) {
        delete m_th;
    }

    for(auto handle : m_udfs) {
        delete handle;
    }

    // Clear queues and delete them
    while(!m_udf_input_queue->empty()) {
        Frame* frame = m_udf_input_queue->front();
        m_udf_input_queue->pop();
        delete frame;
    }
    delete m_udf_input_queue;

    while(!m_udf_output_queue->empty()) {
        Frame* frame = m_udf_output_queue->front();
        m_udf_output_queue->pop();
        delete frame;
    }
    delete m_udf_output_queue;

    delete m_loader;
    delete m_pool;
    config_destroy(m_config);
}

/**
 * Context object for the worker function to get it's frame, UDF handle, and
 * the output queue for resulting frames.
 */
class UdfWorker {
public:
    // UDF to be ran by the worker
    // UdfHandle* handle;

    // Frame for the UDF to process
    Frame* frame;

    // Output queue for the frame (if it not dropped)
    FrameQueue* output_queue;

    // UDF pipeline to continue
    std::vector<UdfHandle*>* udfs;

    /**
     * Constructor
     *
     * @param handle       - UDF handle
     * @param frame        - Frame to process
     * @param output_queue - Output queue to put processed frames into
     */
    UdfWorker(Frame* frame, std::vector<UdfHandle*>* udfs,
              FrameQueue* output_queue) :
        frame(frame), output_queue(output_queue), udfs(udfs)
    {};

    /**
     * Destructor
     */
    ~UdfWorker() {};

    /**
     * UDF worker run method to be executed in a thread pool. This method is
     * static so that it can be passed to the thread pool as a function
     * pointer.
     */
    static void run(void* vargs) {
        UdfWorker* ctx = (UdfWorker*) vargs;

        for(auto handle : *ctx->udfs) {
            UdfRetCode ret = handle->process(ctx->frame);
            // TODO: Should probably have better error reporting here...
            switch(ret) {
                case UdfRetCode::UDF_DROP_FRAME:
                    LOG_DEBUG_0("Dropping frame");
                    delete ctx->frame;
                    return;
                case UdfRetCode::UDF_ERROR:
                    LOG_ERROR_0("Failed to process frame");
                    delete ctx->frame;
                    return;
                case UdfRetCode::UDF_OK:
                    break;
                default:
                    LOG_ERROR_0("Reached default case, this should not happen");
                    delete ctx->frame;
                    return;
            }
        }

        ctx->output_queue->push(ctx->frame);
        delete ctx;

        LOG_DEBUG_0("Done running worker function");
    };
};

void UdfManager::run() {
    LOG_INFO_0("UDFManager thread started");

    // How often to check if the thread should quit
    auto duration = std::chrono::milliseconds(250);

    while(!m_stop.load()) {
        if(m_udf_input_queue->wait_for(duration)) {
            Frame* frame = m_udf_input_queue->front();
            m_udf_input_queue->pop();

            // Create the worker to execute the UDF pipeline on the given frame
            UdfWorker* ctx = new UdfWorker(
                    frame, &m_udfs, m_udf_output_queue);

            // Submit the job to run in the thread pool
            JobHandle* job_handle = NULL;
            do {
                job_handle = m_pool->submit(&UdfWorker::run, ctx);
            } while(job_handle == NULL);

            // The job handle is not actually needed in this use of the
            // thread pool
            delete job_handle;
        }
    }

    LOG_INFO_0("UDFManager thread stopped");
}

void UdfManager::start() {
    if(m_th == NULL && !m_stop.load()) {
        m_th = new std::thread(&UdfManager::run, this);
    } else {
        LOG_WARN_0("Start attempted after stop or after start");
    }
}

void UdfManager::stop() {
    if(m_th != NULL && !m_stop.load()) {
        m_stop.store(true);
        m_th->join();
        m_pool->stop();
    }
}
