// Copyright 2017 Open Source Robotics Foundation, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "depth_to_pointcloud/depth_conversions.hpp"
#include <image_geometry/pinhole_camera_model.h>
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/image_encodings.hpp>
#include <sensor_msgs/msg/camera_info.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <sensor_msgs/point_cloud2_iterator.hpp>

#include <limits>
#include <memory>
#include <string>
#include <vector>

static rclcpp::publisher::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr g_pub_point_cloud;

static sensor_msgs::msg::CameraInfo::SharedPtr g_cam_info;

static void depthCb(const sensor_msgs::msg::Image::SharedPtr image)
{
  // The meat of this function is a port of the code from:
  // https://github.com/ros-perception/image_pipeline/blob/92d7f6b/depth_image_proc/src/nodelets/point_cloud_xyz.cpp

  if (nullptr == g_cam_info) {
    // we haven't gotten the camera info yet, so just drop until we do
    fprintf(stderr, "No camera info, skipping point cloud conversion\n");
    return;
  }

  sensor_msgs::msg::PointCloud2::SharedPtr cloud_msg =
    std::make_shared<sensor_msgs::msg::PointCloud2>();
  cloud_msg->header = image->header;
  cloud_msg->height = image->height;
  cloud_msg->width = image->width;
  cloud_msg->is_dense = false;
  cloud_msg->is_bigendian = false;
  cloud_msg->fields.clear();
  cloud_msg->fields.reserve(1);

  sensor_msgs::PointCloud2Modifier pcd_modifier(*cloud_msg);
  pcd_modifier.setPointCloud2FieldsByString(1, "xyz");

  // g_cam_info here is a sensor_msg::msg::CameraInfo::ConstSharedPtr,
  // which we get from the cam_info topic.
  image_geometry::PinholeCameraModel model;
  model.fromCameraInfo(g_cam_info);

  if (image->encoding == sensor_msgs::image_encodings::TYPE_16UC1) {
    depth_to_pointcloud::convert<uint16_t>(image, cloud_msg, model);
  } else if (image->encoding == sensor_msgs::image_encodings::TYPE_32FC1) {
    depth_to_pointcloud::convert<float>(image, cloud_msg, model);
  } else {
    fprintf(stderr, "Depth image has unsupported encoding [%s]\n", image->encoding.c_str());
    return;
  }

  g_pub_point_cloud->publish(cloud_msg);
}

static void infoCb(sensor_msgs::msg::CameraInfo::SharedPtr info)
{
  g_cam_info = info;
}

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);

  rclcpp::node::Node::SharedPtr node = rclcpp::node::Node::make_shared("depth_to_pointcloud");

  rmw_qos_profile_t custom_qos_profile = rmw_qos_profile_default;

  custom_qos_profile.history = RMW_QOS_POLICY_HISTORY_KEEP_LAST;
  custom_qos_profile.depth = 50;
  custom_qos_profile.reliability = RMW_QOS_POLICY_RELIABILITY_RELIABLE;
  custom_qos_profile.durability = RMW_QOS_POLICY_DURABILITY_VOLATILE;

  g_pub_point_cloud = node->create_publisher<sensor_msgs::msg::PointCloud2>(
    "points2", custom_qos_profile);

  auto image_sub = node->create_subscription<sensor_msgs::msg::Image>(
    "depth", depthCb, custom_qos_profile);
  auto cam_info_sub = node->create_subscription<sensor_msgs::msg::CameraInfo>(
    "depth_camera_info", infoCb, custom_qos_profile);

  rclcpp::spin(node);

  rclcpp::shutdown();

  return 0;
}