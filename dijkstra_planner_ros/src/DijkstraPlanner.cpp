/*
 * DijkstraPlanner.cpp
 *
 *  Created on: Sep 4, 2023
 *      Author: Ikhyeon Cho
 *	 Institute: Korea Univ. ISR (Intelligent Systems & Robotics) Lab
 *       Email: tre0430@korea.ac.kr
 */

#include "dijkstra_planner/DijkstraPlanner.h"

DijkstraPlanner::DijkstraPlanner() : map_is_initialized_{ false }
{
  // Print Node Info
  std::cout << std::endl;
  ROS_INFO("%-65s %s", ("Subscribing to the map topic: " + occupancy_map_topic_).c_str(), "[ DijkstraPlanner]");
  ROS_INFO("%-65s %s", ("Subscribing to the goal topic: " + goal_topic_).c_str(), "[ DijkstraPlanner]");
  std::cout << std::endl;

  if (debug_mode_)
  {
    debug_timer_ = nh_priv_.createTimer(ros::Duration(1.0), &DijkstraPlanner::debug, this, false, false);
    pub_costmap_ = nh_priv_.advertise<grid_map_msgs::GridMap>("debug/costmap", 1);
    pub_occupancy_map_ = nh_priv_.advertise<nav_msgs::OccupancyGrid>("debug/occupancy_map", 1);

    ROS_INFO("%-65s %s", "Debug Mode is activated. Publishing the costmap in 1 Hz...", "[ DijkstraPlanner]");
  }
}

void DijkstraPlanner::initializeMap(const nav_msgs::OccupancyGridConstPtr& msg)
{
  if (map_is_initialized_)
    return;

  OccupancyMapConverter::fromROSMsg(*msg, occupancy_map_);

  // Set grid search space
  wave_propagator_.setSafeDistance(inflation_radius_);
  wave_propagator_.initialize(occupancy_map_, enable_search_unknown_);

  map_is_initialized_ = true;

  if (debug_mode_)
  {
    ROS_INFO("%-65s %s", "Map is initialized. Starting Debug Mode...", "[ DijkstraPlanner]");
    debug_timer_.start();
  }
  else
  {
    ROS_INFO("%-65s %s", "Map is initialized. Waiting for the goal...", "[ DijkstraPlanner]");
  }
}

void DijkstraPlanner::wavePropagation(const geometry_msgs::PoseStampedConstPtr& msg)
{
  ROS_INFO("%-65s %s", "Received a Goal.", "[ DijkstraPlanner]");

  // Check whether the map is received
  if (!map_is_initialized_)
  {
    ROS_WARN("%-65s %s\n", "Map is not received yet --> Check the map topic", "[ DijkstraPlanner]");
    return;
  }

  // Check whether the goal msg is in the map frame - if not, transform it
  if (msg->header.frame_id == map_frame)
  {
    goal_ = *msg;
  }
  else
  {
    auto [has_transform, transform_to_map] = tf_.getTransform(msg->header.frame_id, map_frame);
    if (!has_transform)
    {
      ROS_WARN("%-65s %s\n", "Transform Failed. Goal was not given in map frame!", "[ DijkstraPlanner]");
      return;
    }

    ros_utils::tf::doTransform(*msg, goal_, transform_to_map);
  }

  // Check current robot pose
  auto [has_transform_b2m, base_to_map] = tf_.getTransform(baselink_frame, map_frame);
  if (!has_transform_b2m)
  {
    ROS_WARN("%-65s %s\n", "Could not get the Robot Pose. Please check TF Tree connection!", "[ DijkstraPlanner]");
    return;
  }

  // Convert geometry_msgs::TransformStamped to grid_map::Position
  grid_map::Position goal_position{ goal_.pose.position.x, goal_.pose.position.y };
  grid_map::Position robot_position{ base_to_map.transform.translation.x, base_to_map.transform.translation.y };

  if (!wave_propagator_.search(goal_position, robot_position))
  {
    ROS_ERROR("%-65s %s\n", "No Feasible Path found.", "[ DijkstraPlanner]");
    path_finding_timer_.stop();
    return;
  }

  ROS_INFO("%-65s %s\n", ("Path found. Publishing the path in " + std::to_string(path_update_rate_) + " Hz...").c_str(),
           "[ DijkstraPlanner]");
  path_finding_timer_.start();
}

void DijkstraPlanner::findPath(const ros::TimerEvent& event)
{
  // Set the received goal position
  grid_map::Position goal_position;
  DijkstraPlannerMsgs::fromPoseMsg(goal_, goal_position);

  // Check the validity of the robot position
  // We assume that the robot navigates after planning -> Position is keep changing
  auto [has_transform, base_to_map] = tf_.getTransform(baselink_frame, map_frame);
  if (!has_transform)
  {
    ROS_ERROR("%-65s %s\n", "Could not get robot pose. Please check TF Tree connection!", "[ DijkstraPlanner]");
    return;
  }
  grid_map::Position robot_position;
  robot_position << base_to_map.transform.translation.x, base_to_map.transform.translation.y;

  // Generate current path on the basis of the costmap
  auto [has_path, path] = wave_propagator_.findPath(robot_position, goal_position);
  if (has_path)  // 1. Path to the goal is already searched from the current robot position
  {
    nav_msgs::Path msg_path;
    DijkstraPlannerMsgs::toPathMsg(path, msg_path);
    pub_path_.publish(msg_path);
    return;
  }
  else  // 2. If path generation currently fails, then re-activate wave propagation
  {
    ROS_WARN_THROTTLE(1, "%-65s %s", "Failed to generate path from the costmap. Replanning...", "[ DijkstraPlanner]");
    if (!wave_propagator_.search(goal_position, robot_position))
    {
      ROS_ERROR_THROTTLE(1, "%-65s %s\n", "Could not complete the search. No path available.", "[ DijkstraPlanner]");
      return;
    }

    ROS_INFO("%-65s %s\n", "Replanning done. Found a new path", "[ DijkstraPlanner]");
    return;
  }
}

void DijkstraPlanner::debug(const ros::TimerEvent& event)
{
  // For Debug purpose: Visualize the costmap
  if (pub_costmap_.getNumSubscribers() > 0)
  {
    grid_map_msgs::GridMap costmap_msg;
    grid_map::GridMapRosConverter::toMessage(wave_propagator_.getCostMap(), costmap_msg);
    costmap_msg.info.header.frame_id = map_frame;
    pub_costmap_.publish(costmap_msg);
  }

  // For Debug purpose: Visualize the occupancy map
  if (pub_occupancy_map_.getNumSubscribers() > 0)
  {
    nav_msgs::OccupancyGrid occu_map_msg;
    OccupancyMapConverter::toROSMsg(occupancy_map_, occu_map_msg);
    pub_occupancy_map_.publish(occu_map_msg);
  }
}