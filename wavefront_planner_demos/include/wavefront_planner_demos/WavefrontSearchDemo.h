/*
 * WavefrontSearchDemo.h
 *
 *  Created on: May 5, 2024
 *      Author: Ikhyeon Cho
 *	 Institute: Korea Univ. ISR (Intelligent Systems & Robotics) Lab
 *       Email: tre0430@korea.ac.kr
 */

#ifndef WAVEFRONT_PLANNER_DEMO_H
#define WAVEFRONT_PLANNER_DEMO_H

#include <ros/ros.h>

#include <wavefront_core/wavefront_core.h>
#include <wavefront_msgs/WavefrontMsgs.h>
#include <wavefront_planner_demos/WavePropagatorDemo.h>

class WavefrontSearchDemo
{
public:
  WavefrontSearchDemo();

  void search();

  void publish(const ros::TimerEvent& event);

private:
  ros::NodeHandle nh_priv_{ "~" };

  // Frame Ids
  std::string map_frame{ nh_priv_.param<std::string>("mapFrame", "map") };

  std::string map_img_dir_{ nh_priv_.param<std::string>("mapImgDirectory",
                                                        "/home/ikhyeon/ros/test_ws/src/wavefront_planner/"
                                                        "wavefront_planner_demos/map/maze.png") };

  // Planner Options
  bool enable_search_unknown_{ nh_priv_.param<bool>("propagateUnknownCell", false) };
  double grid_resolution_{ nh_priv_.param<double>("gridResolution", 0.1) };
  double safe_distance_{ nh_priv_.param<double>("safeDistance", 0.0) };
  double goal_x_{ nh_priv_.param<double>("goalX", 1.26) };
  double goal_y_{ nh_priv_.param<double>("goalY", 1.75) };
  double robot_x_{ nh_priv_.param<double>("robotX", 1.2) };
  double robot_y_{ nh_priv_.param<double>("robotY", 0.3) };

  // Duration
  int path_update_rate_{ nh_priv_.param<int>("pathPublishRate", 20) };

  // ROS
  ros::Timer publish_timer_{ nh_priv_.createTimer(ros::Duration(1.0), &WavefrontSearchDemo::publish, this, false,
                                                  false) };
  ros::Publisher pub_costmap_{ nh_priv_.advertise<grid_map_msgs::GridMap>("costmap", 1) };
  ros::Publisher pub_occupancy_map_{ nh_priv_.advertise<nav_msgs::OccupancyGrid>("occupancy_map", 10, true) };

private:
  WavePropagatorDemo wave_propagator_;
  OccupancyMap occupancy_map_;
};

#endif  // WAVEFRONT_PLANNER_DEMO_H