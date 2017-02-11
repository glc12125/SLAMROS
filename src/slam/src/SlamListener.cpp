#include "ros/ros.h"
#include "std_msgs/String.h"
#include "SlamProcessEngine.h"


// TODO: move thi callback to a class and bind to the member function.
// This clqss must knew which SlamProcesEngine to use.
void slamCallback(const std_msgs::String::ConstPtr& msg)
{
  ROS_INFO("I heard: [%s]", msg->data.c_str());
}

int main(int argc, char **argv)
{

  ros::init(argc, argv, "slam_listener");

  ros::NodeHandle n;
  ros::Subscriber sub = n.subscribe("Dji_topic", 1000, slamCallback);

  ros::spin();

  return 0;
}