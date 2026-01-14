#!/bin/bash

tmux new -d -s project11_sim_robot_core
tmux setenv ROS_MASTER_URI http://localhost:11312
tmux send-keys "ROS_MASTER_URI=http://localhost:11312 roscore -p 11312" C-m

export ROS_MASTER_URI=http://localhost:11312
rosrun rosmon rosmon echoboat_project11 sim_echo.launch launchOperator:=false enableBridge:=true
