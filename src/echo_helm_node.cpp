// Roland Arsenault
// Center for Coastal and Ocean Mapping
// University of New Hampshire
// Copyright 2017, All rights reserved.

#include "rclcpp/rclcpp.hpp"
#include "rclcpp_lifecycle/lifecycle_node.hpp"

#include "std_msgs/msg/bool.hpp"
#include "geometry_msgs/msg/twist_stamped.hpp"
#include "project11_msgs/msg/heartbeat.hpp"
#include <mavros_msgs/srv/command_bool.hpp>
#include <mavros_msgs/srv/set_mode.hpp>
#include <mavros_msgs/srv/set_mav_frame.hpp>
#include <mavros_msgs/msg/state.hpp>
#include <mavros_msgs/msg/override_rc_in.hpp>
#include <mavros_msgs/srv/stream_rate.hpp>
#include <mavros_msgs/msg/manual_control.hpp>

using namespace std::chrono_literals;

std::string boolToString(bool value)
{
  if(value)
    return "true";
  return "false";
}

class EchoHelm: public rclcpp_lifecycle::LifecycleNode
{
public:
  explicit EchoHelm()
  :rclcpp_lifecycle::LifecycleNode("echo_helm")
  {

  }
  
  rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
  on_configure(const rclcpp_lifecycle::State &)
  {
    status_publisher_ = create_publisher<project11_msgs::msg::Heartbeat> ("project11/status/helm", 10);
    
    cmd_vel_publisher_ = create_publisher<geometry_msgs::msg::TwistStamped>("mavros/setpoint_velocity/cmd_vel", 10);

    //manual_control_publisher_ = create_publisher<mavros_msgs::msg::ManualControl>("mavros/manual_control/send", 1);
    rc_override_publisher_ = create_publisher<mavros_msgs::msg::OverrideRCIn>("mavros/rc/override", 1);

    cmd_vel_subscription_ = create_subscription<geometry_msgs::msg::TwistStamped>("project11/control/cmd_vel", 10, std::bind(&EchoHelm::cmdVelCallback, this, std::placeholders::_1));

    standby_subscription_ = create_subscription<std_msgs::msg::Bool>("piloting_mode/standby/active", 5, std::bind(&EchoHelm::standbyCallback, this, std::placeholders::_1));
    
    state_subscription_ = create_subscription<mavros_msgs::msg::State>("mavros/state",10, std::bind(&EchoHelm::stateCallback, this, std::placeholders::_1));

    arm_service_client_ = create_client<mavros_msgs::srv::CommandBool>("mavros/cmd/arming");
    mode_service_client_ = create_client<mavros_msgs::srv::SetMode>("mavros/set_mode");
    frame_service_client_ = create_client<mavros_msgs::srv::SetMavFrame>("mavros/setpoint_velocity/mav_frame");
    stream_rate_service_client_ = create_client<mavros_msgs::srv::StreamRate>("mavros/set_stream_rate");
    
    service_cleanup_timer_ = create_wall_timer(5s, std::bind(&EchoHelm::cleanupOldRequests, this));

    return rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn::SUCCESS;
  }

  rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
  on_activate(const rclcpp_lifecycle::State & state)
  {
    LifecycleNode::on_activate(state);
 
    auto stream_rate = std::make_shared<mavros_msgs::srv::StreamRate::Request>();
    stream_rate->message_rate = 10;
    stream_rate->on_off = 1;
    stream_rate_service_client_->async_send_request(stream_rate, std::bind(&EchoHelm::streamRateResponseCallback, this, std::placeholders::_1));

    auto set_mav_frame_request = std::make_shared<mavros_msgs::srv::SetMavFrame::Request>();
    set_mav_frame_request->mav_frame = mavros_msgs::srv::SetMavFrame::Request::FRAME_BODY_NED;
    frame_service_client_->async_send_request(set_mav_frame_request, std::bind(&EchoHelm::mavFrameResponseCallback, this, std::placeholders::_1));

    return rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn::SUCCESS;
  }

  rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
  on_cleanup(const rclcpp_lifecycle::State &)
  {
    status_publisher_.reset();
    cmd_vel_publisher_.reset();
    //manual_control_publisher_.reset();
    rc_override_publisher_.reset();

    cmd_vel_subscription_.reset();
    standby_subscription_.reset();
    state_subscription_.reset();

    arm_service_client_.reset();
    mode_service_client_.reset();
    frame_service_client_.reset();
    stream_rate_service_client_.reset();

    service_cleanup_timer_.reset();

    return rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn::SUCCESS;
  }

private:

  rclcpp::Publisher<project11_msgs::msg::Heartbeat>::SharedPtr status_publisher_;
  rclcpp::Publisher<geometry_msgs::msg::TwistStamped>::SharedPtr cmd_vel_publisher_;
  //rclcpp::Publisher<mavros_msgs::msg::ManualControl>::SharedPtr manual_control_publisher_;
  rclcpp::Publisher<mavros_msgs::msg::OverrideRCIn>::SharedPtr rc_override_publisher_;


  rclcpp::Subscription<geometry_msgs::msg::TwistStamped>::SharedPtr cmd_vel_subscription_;
  rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr standby_subscription_;
  rclcpp::Subscription<mavros_msgs::msg::State>::SharedPtr state_subscription_;


  using CommandBoolClient = rclcpp::Client<mavros_msgs::srv::CommandBool>;

  CommandBoolClient::SharedPtr arm_service_client_;
  rclcpp::Client<mavros_msgs::srv::SetMode>::SharedPtr mode_service_client_;
  rclcpp::Client<mavros_msgs::srv::SetMavFrame>::SharedPtr frame_service_client_;
  rclcpp::Client<mavros_msgs::srv::StreamRate>::SharedPtr stream_rate_service_client_;
  
  rclcpp::TimerBase::SharedPtr service_cleanup_timer_;

  bool standby_ = true;

  void cleanupOldRequests()
  {
    std::vector<int64_t> pruned_requests;
    auto timeout = std::chrono::system_clock::now() - 5s;
    arm_service_client_->prune_requests_older_than(timeout, &pruned_requests);
    if(!pruned_requests.empty())
      RCLCPP_WARN_STREAM(get_logger(), "No response for at least 5 seconds from " << pruned_requests.size() << " arming service request(s)");

    pruned_requests.clear();
    mode_service_client_->prune_requests_older_than(timeout, &pruned_requests);
    if(!pruned_requests.empty())
      RCLCPP_WARN_STREAM(get_logger(), "No response for at least 5 seconds from " << pruned_requests.size() << "set_mode service request(s)");

    pruned_requests.clear();
    frame_service_client_->prune_requests_older_than(timeout, &pruned_requests);
    if(!pruned_requests.empty())
      RCLCPP_WARN_STREAM(get_logger(), "No response for at least 5 seconds from " << pruned_requests.size() << "mav_frame service request(s)");
    
    pruned_requests.clear();
    stream_rate_service_client_->prune_requests_older_than(timeout, &pruned_requests);
    if(!pruned_requests.empty())
      RCLCPP_WARN_STREAM(get_logger(), "No response for at least 5 seconds from " << pruned_requests.size() << "set_stream_rate service request(s)");
  }

  void cmdVelCallback(const geometry_msgs::msg::TwistStamped& msg)
  {
    if(!standby_)
    {
      // scale 2 m/s to full throttle
      double throttle = std::max(-1.0, std::min(1.0, msg.twist.linear.x /2.0));
      // scale 1.5 rad/s to full rudder
      double yaw = std::max(-1.0, std::min(1.0, msg.twist.angular.z / 1.5));

      // turning also seems to make the boat go forwards, so try to reduce that
      double throttle_backoff = -0.02*abs(yaw);
      throttle += throttle_backoff;

      mavros_msgs::msg::OverrideRCIn rc_msg;
      rc_msg.channels[0] = 1500 - (yaw * 400);
      // remove deadband
      if (yaw > 0)
        rc_msg.channels[0] -= 100;
      else if (yaw < 0)
        rc_msg.channels[0] += 100;

      rc_msg.channels[1] = 1500 + (throttle * 420);
      // remove deadband
      if (throttle > 0)
        rc_msg.channels[1] += 80;
      else if (throttle < 0)
        rc_msg.channels[1] -= 80;



      for(std::size_t i = 2; i < rc_msg.channels.size(); i++)
        rc_msg.channels[i] = 0;

      rc_override_publisher_->publish(rc_msg);

      //cmd_vel_publisher_->publish(msg);
    }
  }

  void streamRateResponseCallback(rclcpp::Client<mavros_msgs::srv::StreamRate>::SharedFutureWithRequest future)
  {
    auto request_and_response = future.get();
  }

  void mavFrameResponseCallback(rclcpp::Client<mavros_msgs::srv::SetMavFrame>::SharedFutureWithRequest future)
  {
    auto request_and_response = future.get();
  }

  void armingResponseCallback(CommandBoolClient::SharedFutureWithRequest future)
  {
    auto request_and_response = future.get();
  }

  void setModeResponseCallback(rclcpp::Client<mavros_msgs::srv::SetMode>::SharedFutureWithRequest future)
  {
    auto request_and_response = future.get();
  }

  void armFromGuidedCallback(rclcpp::Client<mavros_msgs::srv::SetMode>::SharedFutureWithRequest future)
  {
    auto request_and_response = future.get();
    auto arm_request = std::make_shared<mavros_msgs::srv::CommandBool::Request>();
    arm_request->value = true;
    arm_service_client_->async_send_request(arm_request, std::bind(&EchoHelm::armingResponseCallback, this, std::placeholders::_1));
  }


  void disarmedForGuidedCallback(CommandBoolClient::SharedFutureWithRequest future)
  {
    auto request_and_response = future.get();

    auto set_mode_request = std::make_shared<mavros_msgs::srv::SetMode::Request>();
    set_mode_request->custom_mode = "GUIDED";
    mode_service_client_->async_send_request(set_mode_request, std::bind(&EchoHelm::armFromGuidedCallback, this, std::placeholders::_1));
  }

  void disarmedForManualCallback(CommandBoolClient::SharedFutureWithRequest future)
  {
    auto request_and_response = future.get();

    auto set_mode_request = std::make_shared<mavros_msgs::srv::SetMode::Request>();
    set_mode_request->custom_mode = "MANUAL";
    mode_service_client_->async_send_request(set_mode_request, std::bind(&EchoHelm::armFromGuidedCallback, this, std::placeholders::_1));
  }


  void standbyCallback(const std_msgs::msg::Bool& msg)
  {
    // from standby to active
    if(standby_ && !msg.data)
    {
      auto set_mav_frame_request = std::make_shared<mavros_msgs::srv::SetMavFrame::Request>();
      set_mav_frame_request->mav_frame = mavros_msgs::srv::SetMavFrame::Request::FRAME_BODY_NED;
      frame_service_client_->async_send_request(set_mav_frame_request, std::bind(&EchoHelm::mavFrameResponseCallback, this, std::placeholders::_1));
  
      auto arm_request = std::make_shared<mavros_msgs::srv::CommandBool::Request>();
      arm_request->value = false;
      //arm_service_client_->async_send_request(arm_request, std::bind(&EchoHelm::disarmedForGuidedCallback, this, std::placeholders::_1));
      arm_service_client_->async_send_request(arm_request, std::bind(&EchoHelm::disarmedForManualCallback, this, std::placeholders::_1));
    }
    standby_ = msg.data;
    // to standby
    if(msg.data)
    {
      auto arm_request = std::make_shared<mavros_msgs::srv::CommandBool::Request>();
      arm_request->value = false;
      arm_service_client_->async_send_request(arm_request, std::bind(&EchoHelm::disarmedForManualCallback, this, std::placeholders::_1));
    }
  }

  void stateCallback(const mavros_msgs::msg::State& inmsg)
  {
    project11_msgs::msg::Heartbeat hb;
    hb.header = inmsg.header;

    project11_msgs::msg::KeyValue kv;

    kv.key = "project11_standby";
    kv.value = boolToString(standby_);
    hb.values.push_back(kv);
    
    kv.key = "connected";
    kv.value = boolToString(inmsg.connected);
    hb.values.push_back(kv);

    kv.key = "armed";
    kv.value = boolToString(inmsg.armed);
    hb.values.push_back(kv);

    kv.key = "guided";
    kv.value = boolToString(inmsg.guided);
    hb.values.push_back(kv);

    kv.key = "mode";
    kv.value = inmsg.mode;
    hb.values.push_back(kv);
    
    status_publisher_->publish(hb);
  }
};


int main(int argc, char **argv)
{
  rclcpp::init(argc, argv);
  auto helm = std::make_shared<EchoHelm>();
  rclcpp::executors::SingleThreadedExecutor exe;
  exe.add_node(helm->get_node_base_interface());
  exe.spin();
  
  rclcpp::shutdown();

  return 0;
}
