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
 * @file
 * @brief RealSense UDF Implementation
 */

#include <eii/udf/base_udf.h>
#include <eii/utils/logger.h>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <librealsense2/rs.hpp>
#include <librealsense2/hpp/rs_internal.hpp>

#define COLOR_BYTES_PER_PIXEL 3
#define DEPTH_BYTES_PER_PIXEL 2

using namespace eii::udf;

// Software_device can be used to generate frames from synthetic or
// external sources and pass them into SDK processing functionality.
// Create software-only device
rs2::software_device dev;

// Define sensor for respective stream
auto depth_sensor = dev.add_sensor("Depth");
auto color_sensor = dev.add_sensor("Color");

namespace eii {
    namespace udfsamples {

    /**
     * The RealSense UDF
     */
        class RealSenseUdf : public BaseUdf {
            private:
                // Depth intrinsics width
                int m_depth_width;

                // Depth intrinsics height
                int m_depth_height;

                // Depth intrinsics x-principal-point
                float m_depth_ppx;

                // Depth intrinsics y-principal-point
                float m_depth_ppy;

                // Depth intrinsics x-focal-point
                float m_depth_fx;

                // Depth intrinsics y-focal-point
                float m_depth_fy;

                // Depth intrinsics model
                int m_depth_model;

                // Color intrinsics width
                int m_color_width;

                // Color instrinsics height
                int m_color_height;

                // Color intrinsics x-principal-point
                float m_color_ppx;

                // Color intrinsics y-prinpical-point
                float m_color_ppy;

                // Color intrinsics x-focal-point
                float m_color_fx;

                // Color intrinsics y-focal-point
                float m_color_fy;

                // Color intrinsics model
                int m_color_model;

                // Frame number for synchronization
                int m_frame_number;

                // Frame struct
                struct software_device_frame
                {
                    int x, y, bpp;
                    std::vector<uint8_t> frame;
                };

                software_device_frame m_sw_depth_frame;
                software_device_frame m_sw_color_frame;

                // For storing the profile of stream
                rs2::stream_profile m_depth_stream;
                rs2::stream_profile m_color_stream;

                // For Synchronizing frames using the syncer class
                rs2::syncer m_sync;

            public:
                RealSenseUdf(config_t* config): BaseUdf(config) {

                    m_frame_number = 0;

                };

                ~RealSenseUdf() {};

                UdfRetCode process(cv::Mat& frame, cv::Mat& output, msg_envelope_t* meta) override {

                    LOG_DEBUG_0("Inside RealSense UDF process function");

                    set_rs2_intrinsics(meta);

                    rs2::frameset fset = construct_rs2_frameset(frame.data);

                    ++m_frame_number;

                    // Return first found frame for the stream specified
                    // rs2::depth_frame rs2_depth = fset.first_or_default(RS2_STREAM_DEPTH);
                    rs2::video_frame rs2_color = fset.first_or_default(RS2_STREAM_COLOR);

                    // Do processing on depth frame

                    return UdfRetCode::UDF_OK;
            };

            void set_rs2_intrinsics(msg_envelope_t* meta) {
                if (m_frame_number < 1) {

                    msgbus_ret_t ret;

                    // Get depth intrinsics width
                    msg_envelope_elem_body_t* depth_intrinsics_width = NULL;
                    ret = msgbus_msg_envelope_get(meta, "rs2_depth_intrinsics_width", &depth_intrinsics_width);
                    if(ret != MSG_SUCCESS) {
                        throw "Failed to retrieve depth intrinsics width";
                    } else if(depth_intrinsics_width->type != MSG_ENV_DT_INT) {
                        throw "depth instrinsics width must be an integer";
                    }
                    m_depth_width = depth_intrinsics_width->body.integer;


                    // Get depth intrinsics height
                    msg_envelope_elem_body_t* depth_intrinsics_height = NULL;
                    ret = msgbus_msg_envelope_get(meta, "rs2_depth_intrinsics_height", &depth_intrinsics_height);
                    if(ret != MSG_SUCCESS) {
                        throw "Failed to retrieve depth intrinsics height";
                    } else if(depth_intrinsics_height->type != MSG_ENV_DT_INT) {
                        throw "depth intrinsics height must be an integer";
                    }
                    m_depth_height = depth_intrinsics_height->body.integer;

                    // Get depth intrinsics x-principal-point
                    msg_envelope_elem_body_t* depth_intrinsics_ppx = NULL;
                    ret = msgbus_msg_envelope_get(meta, "rs2_depth_intrinsics_ppx", &depth_intrinsics_ppx);
                    if(ret != MSG_SUCCESS) {
                        throw "Failed to retrieve depth intrinsics x-principal-point";
                    } else if(depth_intrinsics_ppx->type != MSG_ENV_DT_FLOATING) {
                        throw "depth intrinsics x-principal-point must be a floating";
                    }
                    m_depth_ppx = depth_intrinsics_ppx->body.floating;

                    // Get depth intrinsics y-principal-point
                    msg_envelope_elem_body_t* depth_intrinsics_ppy = NULL;
                    ret = msgbus_msg_envelope_get(meta, "rs2_depth_intrinsics_ppy", &depth_intrinsics_ppy);
                    if(ret != MSG_SUCCESS) {
                        throw "Failed to retrieve depth intrinsics y-principal-point";
                    } else if(depth_intrinsics_ppy->type != MSG_ENV_DT_FLOATING) {
                        throw "depth intrinsics y-principal-point must be a floating";
                    }
                    m_depth_ppy = depth_intrinsics_ppy->body.floating;

                    // Get depth intrinsics x-focal-point
                    msg_envelope_elem_body_t* depth_intrinsics_fx = NULL;
                    ret = msgbus_msg_envelope_get(meta, "rs2_depth_intrinsics_fx", &depth_intrinsics_fx);
                    if(ret != MSG_SUCCESS) {
                        throw "Failed to retrieve depth intrinsics x-focal-point";
                    } else if(depth_intrinsics_fx->type != MSG_ENV_DT_FLOATING) {
                        throw "depth intrinsics x-focal-point must be a floating";
                    }
                    m_depth_fx = depth_intrinsics_fx->body.floating;

                    // Get depth intrinsics y-focal-point
                    msg_envelope_elem_body_t* depth_intrinsics_fy = NULL;
                    ret = msgbus_msg_envelope_get(meta, "rs2_depth_intrinsics_fy", &depth_intrinsics_fy);
                    if(ret != MSG_SUCCESS) {
                        throw "Failed to retrieve depth intrinsics y-focal-point";
                    } else if(depth_intrinsics_ppy->type != MSG_ENV_DT_FLOATING) {
                        throw "color intrinsics y-focal-point must be a floating";
                    }
                    m_depth_fy = depth_intrinsics_fy->body.floating;

                    // Get depth intrinsics model
                    msg_envelope_elem_body_t* depth_intrinsics_model = NULL;
                    ret = msgbus_msg_envelope_get(meta, "rs2_depth_intrinsics_model", &depth_intrinsics_model);
                    if(ret != MSG_SUCCESS) {
                        throw "Failed to retrieve depth intrinsics model";
                    } else if(depth_intrinsics_model->type != MSG_ENV_DT_INT) {
                        throw "depth intrinsics model must be an integer";
                    }
                    m_depth_model = depth_intrinsics_model->body.integer;

                     // Get color intrinsics width
                    msg_envelope_elem_body_t* color_intrinsics_width = NULL;
                    ret = msgbus_msg_envelope_get(meta, "rs2_color_intrinsics_width", &color_intrinsics_width);
                    if(ret != MSG_SUCCESS) {
                        throw "Failed to retrieve color intrinsics width";
                    } else if(color_intrinsics_width->type != MSG_ENV_DT_INT) {
                        throw "Color frame height must be a floating";
                    }
                    m_color_width = color_intrinsics_width->body.integer;

                    // Get color intrinsics height
                    msg_envelope_elem_body_t* color_intrinsics_height = NULL;
                    ret = msgbus_msg_envelope_get(meta, "rs2_color_intrinsics_height", &color_intrinsics_height);
                    if(ret != MSG_SUCCESS) {
                        throw "Failed to retrieve color intrinsics height";
                    } else if(color_intrinsics_height->type != MSG_ENV_DT_INT) {
                        throw "color intrinsics height must be a floating";
                    }
                    m_color_height = color_intrinsics_height->body.integer;

                    // Get color intrinsics x-principal-point
                    msg_envelope_elem_body_t* color_intrinsics_ppx = NULL;
                    ret = msgbus_msg_envelope_get(meta, "rs2_color_intrinsics_ppx", &color_intrinsics_ppx);
                    if(ret != MSG_SUCCESS) {
                        throw "Failed to retrieve color intrinsics x-principal-point";
                    } else if(color_intrinsics_ppx->type != MSG_ENV_DT_FLOATING) {
                        throw "color intrinsics x-principal-point must be a floating";
                    }
                    m_color_ppx = color_intrinsics_ppx->body.floating;

                    // Get color intrinsics y-principal-point
                    msg_envelope_elem_body_t* color_intrinsics_ppy = NULL;
                    ret = msgbus_msg_envelope_get(meta, "rs2_color_intrinsics_ppy", &color_intrinsics_ppy);
                    if(ret != MSG_SUCCESS) {
                        throw "Failed to retrieve color intrinsics y-principal-point";
                    } else if(color_intrinsics_ppy->type != MSG_ENV_DT_FLOATING) {
                        throw "color intrinsics y-principal-point must be a floating";
                    }
                    m_color_ppy = color_intrinsics_ppy->body.floating;

                    // Get color intrinsics x-focal-point
                    msg_envelope_elem_body_t* color_intrinsics_fx = NULL;
                    ret = msgbus_msg_envelope_get(meta, "rs2_color_intrinsics_fx", &color_intrinsics_fx);
                    if(ret != MSG_SUCCESS) {
                        throw "Failed to retrieve color intrinsics x-focal-point";
                    } else if(color_intrinsics_fx->type != MSG_ENV_DT_FLOATING) {
                        throw "color intrinsics x-focal-point must be a floating";
                    }
                    m_color_fx = color_intrinsics_fx->body.floating;

                    // Get color intrinsics y-focal-point
                    msg_envelope_elem_body_t* color_intrinsics_fy = NULL;
                    ret = msgbus_msg_envelope_get(meta, "rs2_color_intrinsics_fy", &color_intrinsics_fy);
                    if(ret != MSG_SUCCESS) {
                        throw "Failed to retrieve color intrinsics y-focal-point";
                    } else if(color_intrinsics_ppy->type != MSG_ENV_DT_FLOATING) {
                        throw "color intrinsics y-focal-point must be a floating";
                    }
                    m_color_fy = color_intrinsics_fy->body.floating;

                    // Get color intrinsics model
                    msg_envelope_elem_body_t* color_intrinsics_model = NULL;
                    ret = msgbus_msg_envelope_get(meta, "rs2_color_intrinsics_model", &color_intrinsics_model);
                    if(ret != MSG_SUCCESS) {
                        throw "Failed to retrieve color intrinsics model";
                    } else if(color_intrinsics_model->type != MSG_ENV_DT_INT) {
                        throw "depth intrinsics model must be an integer";
                    }
                    m_color_model = color_intrinsics_model->body.integer;

                    m_sw_depth_frame.x = m_depth_width;
                    m_sw_depth_frame.y = m_depth_height;
                    m_sw_depth_frame.bpp = DEPTH_BYTES_PER_PIXEL;

                    m_sw_color_frame.x = m_color_width;
                    m_sw_color_frame.y = m_color_height;
                    m_sw_color_frame.bpp = COLOR_BYTES_PER_PIXEL;

                    // Before passing images into the device, provide details about the
                    // stream which is going to be simulated
                    rs2_intrinsics depth_intrinsics = { m_sw_depth_frame.x,
                        m_sw_depth_frame.y,
                        (float)m_depth_ppx,
                        (float)m_depth_ppy,
                        (float)m_depth_fx ,
                        (float)m_depth_fy ,
                        (rs2_distortion)m_depth_model,
                        { 0,0,0,0,0 }};

                    m_depth_stream = depth_sensor.add_video_stream({ RS2_STREAM_DEPTH,
                        0,
                        0,
                        m_depth_width,
                        m_depth_height,
                        60,
                        DEPTH_BYTES_PER_PIXEL,
                        RS2_FORMAT_Z16,
                        depth_intrinsics });

                    rs2_intrinsics color_intrinsics = { m_sw_color_frame.x,
                        m_sw_color_frame.y,
                        (float)m_color_ppx,
                        (float)m_color_ppy,
                        (float)m_color_fx,
                        (float)m_color_fy,
                        (rs2_distortion)m_color_model,
                        { 0,0,0,0,0 }};

                    m_color_stream = color_sensor.add_video_stream({ RS2_STREAM_COLOR,
                        0,
                        1,
                        m_color_width,
                        m_color_height,
                        60,
                        COLOR_BYTES_PER_PIXEL,
                        RS2_FORMAT_RGB8,
                        color_intrinsics });


                    // Adding sensor options
                    depth_sensor.add_read_only_option(RS2_OPTION_DEPTH_UNITS, 0.001f);

                    // Specify synochronization model for using syncer class with synthetic streams
                    dev.create_matcher(RS2_MATCHER_DLR_C);

                    // Open the sensor for respective stream
                    depth_sensor.open(m_depth_stream);
                    color_sensor.open(m_color_stream);

                    // Start the sensor for passing frame to the syncer callback
                    depth_sensor.start(m_sync);
                    color_sensor.start(m_sync);

                    // Assign extrinsic transformation parameters to a specific profile (sensor)
                    m_depth_stream.register_extrinsics_to(m_color_stream, { { 1,0,0,0,1,0,0,0,1 },{ 0,0,0 } });
                }

            }

            rs2::frameset construct_rs2_frameset(void* color) {


                /* TODO: Verify with multi frame support
                depth_sensor.on_video_frame({(void *)depth_frame, // Frame pixels from capture API
                    [](void*) {}, // Custom deleter (if required)
                    m_sw_depth_frame.x * m_sw_depth_frame.bpp,
                    m_sw_depth_frame.bpp, // Stride and Bytes-per-pixel
                    (rs2_time_t)m_frame_number * 16,
                    RS2_TIMESTAMP_DOMAIN_HARDWARE_CLOCK, // Timestamp
                    m_frame_number, // Frame# for potential m_sync services
                    m_depth_stream});
                */

                color_sensor.on_video_frame({color, // Frame pixels from capture API
                    [](void*) {}, // Custom deleter (if required)
                    m_sw_color_frame.x * m_sw_color_frame.bpp,
                    m_sw_color_frame.bpp, // Stride and Bytes-per-pixel
                    (rs2_time_t)m_frame_number * 16,
                    RS2_TIMESTAMP_DOMAIN_HARDWARE_CLOCK, // Timestamp
                    m_frame_number, // Frame# for potential m_sync services
                    m_color_stream});

                    // Wait for frameset from syncer class
                    rs2::frameset fset = m_sync.wait_for_frames();

                    return fset;

            }

        };
    } // udf
} // eii

extern "C" {

/**
 * Create the UDF.
 *
 * @return void*
 */
void* initialize_udf(config_t* config) {
    eii::udfsamples::RealSenseUdf* udf = new eii::udfsamples::RealSenseUdf(config);
    return (void*) udf;
}

} // extern "C"


