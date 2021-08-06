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
* @brief RealSense UDF Implementation to simulate and work with fisheye frames
* and extract the pose data
*/

#include <eii/udf/raw_base_udf.h>
#include <eii/utils/logger.h>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <librealsense2/rs.hpp>
#include <librealsense2/hpp/rs_internal.hpp>

#define COLOR_BYTES_PER_PIXEL 3
#define DEPTH_BYTES_PER_PIXEL 2
#define COLOR_FRAME_CHANNELS 3
// This is the order in which EII ingestion pushes frame.
#define FISHEYE1_FRAME_INDEX 0
#define FISHEYE2_FRAME_INDEX 1

using namespace eii::udf;
using namespace cv;

// Software_device can be used to generate frames from synthetic or
// external sources and pass them into SDK processing functionality.
// Create software-only device
rs2::software_device dev;

// Define sensor for respective stream
auto m_fisheye1_sensor = dev.add_sensor("Fisheye1");
auto m_fisheye2_sensor = dev.add_sensor("Fisheye2");

// Frame queues for color and depth frames
rs2::frame_queue m_fisheye1_queue;
rs2::frame_queue m_fisheye2_queue;

namespace eii {
    namespace udfsamples {

        /**
        * Realsense sample UDF for tracking camera
        * Simulates fisheye frames using rs2 software device
        * Extracts pose data from frame metadata
        */
        class RealSenseTrackingUdf : public RawBaseUdf {
            private:
                // Colorizing the depth frame.
                rs2::colorizer color_map;
                // Frame number for synchronization
                int m_frame_number;
                // Frame struct
                struct software_device_frame
                {
                    int x, y, bpp;
                    std::vector<uint8_t> frame;
                };

                software_device_frame m_sw_fisheye1;
                software_device_frame m_sw_fisheye2;

                // For storing the profile of stream
                rs2::stream_profile m_fisheye1_stream;
                rs2::stream_profile m_fisheye2_stream;

                typedef struct rs2_vector
                {
                    float x, y, z;
                }rs2_vector;

                typedef struct rs2_quaternion
                {
                    float x, y, z, w;
                }rs2_quaternion;

                typedef struct rs2_pose
                {
                    rs2_vector      translation;
                    rs2_vector      velocity;
                    rs2_vector      acceleration;
                    rs2_quaternion  rotation;
                    rs2_vector      angular_velocity;
                    rs2_vector      angular_acceleration;
                    unsigned int    tracker_confidence;
                    unsigned int    mapper_confidence;
                }rs2_pose;

                rs2_pose pose;

            public:
                RealSenseTrackingUdf(config_t* config): RawBaseUdf(config) {

                    m_frame_number = 0;

                };

                ~RealSenseTrackingUdf() {};

                UdfRetCode process(Frame* frame) override {

                    LOG_DEBUG_0("Inside RealSenseTracking UDF process function");

                    // Set intrinsics and extrinsics values
                    set_rs2_intrinsics_and_extrinsics();

                    void* fisheye1_frame = frame->get_data(FISHEYE1_FRAME_INDEX);
                    if (fisheye1_frame == NULL) {
                        LOG_ERROR_0("fisheye1_frame is NULL");
                    }
                    void* fisheye2_frame = frame->get_data(FISHEYE2_FRAME_INDEX);
                    if (fisheye2_frame == NULL) {
                        LOG_ERROR_0("fisheye2_frame is NULL");
                    }

                    // Construct fisheye frames using rs2 software_device
                    construct_rs2_frameset(fisheye1_frame, fisheye2_frame);

                    // Get the pose data from frame metadata
                    get_pose_metadata(frame->get_meta_data());

                    ++m_frame_number;

                    // Wait until new frame becomes available in the queue and dequeue it
                    rs2::video_frame rs2_fisheye1 = m_fisheye1_queue.wait_for_frame();
                    rs2::video_frame rs2_fisheye2 = m_fisheye2_queue.wait_for_frame();

                    if ((rs2_fisheye1 == NULL) || (rs2_fisheye2 == NULL)) {
                        LOG_ERROR_0("fisheye1 or fisheye2 frame returned NULL");
                    }

                    // Do processing on fisheye frame
                    const int fisheye1_width = rs2_fisheye1.as<rs2::video_frame>().get_width();
                    const int fisheye1_height = rs2_fisheye1.as<rs2::video_frame>().get_height();

                    LOG_DEBUG("Fisheye1 width: %d", fisheye1_width);
                    LOG_DEBUG("Fisheye1 height: %d", fisheye1_height);
                    LOG_DEBUG("Pose Acceleration: x:%f, y:%f, z:%f", pose.acceleration.x, pose.acceleration.y, pose.acceleration.z);
                    LOG_DEBUG("Pose Angular Acceleration: x:%f, y:%f, z:%f", pose.angular_acceleration.x, pose.angular_acceleration.y, pose.angular_acceleration.z);
                    LOG_DEBUG("Pose Angular Velocity: x:%f, y:%f, z:%f", pose.angular_velocity.x, pose.angular_velocity.y, pose.angular_velocity.z);
                    LOG_DEBUG("Pose Translation: x:%f, y:%f, z:%f", pose.translation.x, pose.translation.y, pose.translation.z);
                    LOG_DEBUG("Pose Velocity: x:%f, y:%f, z:%f", pose.velocity.x, pose.velocity.y, pose.velocity.z);
                    LOG_DEBUG("Pose Rotation: x:%f, y:%f, z:%f, w:%f", pose.rotation.x, pose.rotation.y, pose.rotation.z, pose.rotation.w);
                    LOG_DEBUG("Pose Tracker Confidence: %d", pose.tracker_confidence);
                    LOG_DEBUG("Pose Mapper Confidence: %d", pose.mapper_confidence);

                    return UdfRetCode::UDF_OK;
                };

            static void free_cv_frame(void* obj) {
                cv::Mat* frame = (cv::Mat*) obj;
                frame->release();
                delete frame;
            }

            void set_rs2_intrinsics_and_extrinsics() {
                if (m_frame_number < 1) {

                    m_sw_fisheye1.x = 848;
                    m_sw_fisheye1.y = 800;
                    m_sw_fisheye1.bpp = 1;

                    m_sw_fisheye2.x = 848;
                    m_sw_fisheye2.y = 800;
                    m_sw_fisheye2.bpp = 1;

                    rs2_intrinsics fisheye2_intrinsics = { m_sw_fisheye1.x,
                        m_sw_fisheye1.y,
                        (float)424.126,
                        (float)405.984,
                        (float)285.634,
                        (float)285.734,
                        RS2_DISTORTION_KANNALA_BRANDT4,
                        { 0,0,0,0,0 } };

                    rs2_intrinsics fisheye1_intrinsics = { m_sw_fisheye2.x,
                        m_sw_fisheye2.y,
                        (float)431.302,
                        (float)403.731,
                        (float)286.335,
                        (float)286.179,
                        RS2_DISTORTION_KANNALA_BRANDT4,
                        { 0,0,0,0,0 } };


                    m_fisheye1_stream = m_fisheye1_sensor.add_video_stream({  RS2_STREAM_FISHEYE, 0, 0,
                                                m_sw_fisheye1.x, m_sw_fisheye1.y, 60, m_sw_fisheye1.bpp,
                                                RS2_FORMAT_Y8, fisheye1_intrinsics });

                    m_fisheye2_stream = m_fisheye2_sensor.add_video_stream({  RS2_STREAM_FISHEYE, 0, 1,
                                                m_sw_fisheye2.x, m_sw_fisheye2.y, 60, m_sw_fisheye2.bpp,
                                                RS2_FORMAT_Y8, fisheye2_intrinsics });

                    m_fisheye1_stream.register_extrinsics_to(m_fisheye2_stream, { { 1,0,0,0,1,0,0,0,1 },{ 0,0,0 } });

                    dev.create_matcher(RS2_MATCHER_DLR_C);

                    m_fisheye1_sensor.open(m_fisheye1_stream);
                    m_fisheye2_sensor.open(m_fisheye2_stream);


                    m_fisheye1_sensor.start(m_fisheye1_queue);
                    m_fisheye2_sensor.start(m_fisheye2_queue);
                }

            }

            void construct_rs2_frameset(void* fisheye1, void* fisheye2) {

                //SW Device Simulation
                m_fisheye1_sensor.on_video_frame({fisheye1, // Frame pixels from capture API
                    [](void*) {}, // Custom deleter (if required)
                    m_sw_fisheye1.x * m_sw_fisheye1.bpp,
                    m_sw_fisheye1.bpp, // Stride and Bytes-per-pixel
                    (rs2_time_t)m_frame_number * 16,
                    RS2_TIMESTAMP_DOMAIN_HARDWARE_CLOCK, m_frame_number, // Timestamp, Frame# for potential sync services
                    m_fisheye1_stream});


                m_fisheye2_sensor.on_video_frame({fisheye2, // Frame pixels from capture API
                    [](void*) {}, // Custom deleter (if required)
                    m_sw_fisheye2.x * m_sw_fisheye2.bpp,
                    m_sw_fisheye2.bpp, // Stride and Bytes-per-pixel
                    (rs2_time_t)m_frame_number * 16,
                    RS2_TIMESTAMP_DOMAIN_HARDWARE_CLOCK, m_frame_number, // Timestamp, Frame# for potential sync services
                    m_fisheye2_stream});

            }

            void get_pose_metadata(msg_envelope_t* meta) {

                msgbus_ret_t ret;

                msg_envelope_elem_body_t* x = NULL;
                msg_envelope_elem_body_t* y = NULL;
                msg_envelope_elem_body_t* z = NULL;
                msg_envelope_elem_body_t* w = NULL;


                msg_envelope_elem_body_t* acc = NULL;
                ret = msgbus_msg_envelope_get(meta, "acceleration", &acc);
                x = msgbus_msg_envelope_elem_array_get_at(acc, 0);
                pose.acceleration.x = x->body.floating;
                y = msgbus_msg_envelope_elem_array_get_at(acc, 1);
                pose.acceleration.y = y->body.floating;
                z = msgbus_msg_envelope_elem_array_get_at(acc, 2);
                pose.acceleration.z = z->body.floating;

                msg_envelope_elem_body_t* ang_acc = NULL;
                ret = msgbus_msg_envelope_get(meta, "angular_acceleration", &ang_acc);
                x = msgbus_msg_envelope_elem_array_get_at(ang_acc, 0);
                pose.angular_acceleration.x = x->body.floating;
                y = msgbus_msg_envelope_elem_array_get_at(ang_acc, 1);
                pose.angular_acceleration.y = y->body.floating;
                z = msgbus_msg_envelope_elem_array_get_at(ang_acc, 2);
                pose.angular_acceleration.z = z->body.floating;

                msg_envelope_elem_body_t* ang_vel = NULL;
                ret = msgbus_msg_envelope_get(meta, "angular_velocity", &ang_vel);
                x = msgbus_msg_envelope_elem_array_get_at(ang_vel, 0);
                pose.angular_velocity.x = x->body.floating;
                y = msgbus_msg_envelope_elem_array_get_at(ang_vel, 1);
                pose.angular_velocity.y = y->body.floating;
                z = msgbus_msg_envelope_elem_array_get_at(ang_vel, 2);
                pose.angular_velocity.z = z->body.floating;

                msg_envelope_elem_body_t* tran = NULL;
                ret = msgbus_msg_envelope_get(meta, "translation", &tran);
                x = msgbus_msg_envelope_elem_array_get_at(tran, 0);
                pose.translation.x = x->body.floating;
                y = msgbus_msg_envelope_elem_array_get_at(tran, 1);
                pose.translation.y = y->body.floating;
                z = msgbus_msg_envelope_elem_array_get_at(tran, 2);
                pose.translation.z = z->body.floating;

                msg_envelope_elem_body_t* vel = NULL;
                ret = msgbus_msg_envelope_get(meta, "velocity", &vel);
                x = msgbus_msg_envelope_elem_array_get_at(vel, 0);
                pose.velocity.x = x->body.floating;
                y = msgbus_msg_envelope_elem_array_get_at(vel, 1);
                pose.velocity.y = y->body.floating;
                z = msgbus_msg_envelope_elem_array_get_at(vel, 2);
                pose.velocity.z = z->body.floating;

                msg_envelope_elem_body_t* rot = NULL;
                ret = msgbus_msg_envelope_get(meta, "rotation", &rot);
                x = msgbus_msg_envelope_elem_array_get_at(rot, 0);
                pose.rotation.x = x->body.floating;
                y = msgbus_msg_envelope_elem_array_get_at(rot, 1);
                pose.rotation.y = y->body.floating;
                z = msgbus_msg_envelope_elem_array_get_at(rot, 2);
                pose.rotation.z = z->body.floating;
                w = msgbus_msg_envelope_elem_array_get_at(rot, 3);
                pose.rotation.w = w->body.floating;

                msg_envelope_elem_body_t* tc = NULL;
                ret = msgbus_msg_envelope_get(meta, "tracker_confidence", &tc);
                pose.tracker_confidence = tc->body.integer;

                msg_envelope_elem_body_t* mc = NULL;
                ret = msgbus_msg_envelope_get(meta, "mapper_confidence", &mc);
                pose.mapper_confidence = mc->body.integer;

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
    eii::udfsamples::RealSenseTrackingUdf* udf = new eii::udfsamples::RealSenseTrackingUdf(config);
    return (void*) udf;
}

} // extern "C"
