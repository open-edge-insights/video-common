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


void free_fn(void* ptr) {
    // Do nothing..
}

config_value_t* get_config_value(const void* cfg, const char* key) {
    config_value_t* obj = (config_value_t*) cfg;
    return config_value_object_get(obj, key);
}

UdfManager::UdfManager(
        config_t* udf_cfg, FrameQueue* input_queue, FrameQueue* output_queue) :
    m_th(NULL), m_stop(false), m_config(udf_cfg),
    m_udf_input_queue(input_queue), m_udf_output_queue(output_queue)
{
    m_loader = new UdfLoader();

    LOG_DEBUG_0("Loading UDFs");
    config_value_t* udfs = config_get(m_config, "udfs");
    if(udfs == NULL) {
        delete m_loader;
        throw "Failed to get UDFs";
    }
    if(udfs->type != CVT_ARRAY) {
        delete m_loader;
        config_value_destroy(udfs);
        throw "\"udfs\" must be an array";
    }

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
    config_destroy(m_config);
}

void UdfManager::run() {
    LOG_INFO_0("UDFManager thread started");

    // How often to check if the thread should quit
    auto duration = std::chrono::milliseconds(250);

    while(!m_stop.load()) {
        if(m_udf_input_queue->wait_for(duration)) {
            Frame* frame = m_udf_input_queue->front();
            m_udf_input_queue->pop();
            // TODO: Use thread queue...
            for(auto handle : m_udfs) {
                UdfRetCode ret = handle->process(frame);
                switch(ret) {
                    case UdfRetCode::UDF_DROP_FRAME:
                        LOG_DEBUG_0("Dropping frame");
                        delete frame;
                        break;
                    case UdfRetCode::UDF_ERROR:
                        LOG_ERROR_0("Failed to process frame");
                        delete frame;
                        break;
                    case UdfRetCode::UDF_OK:
                        m_udf_output_queue->push(frame);
                        break;
                    case UdfRetCode::UDF_MODIFIED_FRAME: // This ret code should be removed
                    default:
                        LOG_ERROR_0("Reached default case, this should not happen");
                        delete frame;
                        break;
                }
            }
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
    }
}
