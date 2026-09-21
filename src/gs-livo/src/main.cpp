#include "LIVMapper.h"

int main(int argc, char **argv)
{
  rclcpp::init(argc, argv);
  auto mapper = std::make_shared<LIVMapper>();
  image_transport::ImageTransport it(mapper);
  mapper->initializeSubscribersAndPublishers(it);
  mapper->run();
  rclcpp::shutdown();
  return 0;
}
