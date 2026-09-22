#include "LIVMapper.h"

int main(int argc, char **argv)
{
  rclcpp::init(argc, argv);
  // Build LIVMapper via make_shared so its enable_shared_from_this::weak_this
  // is initialized by the time initializeCamera() runs. Calling shared_from_this()
  // from LIVMapper's own constructor body (as the original initializeComponents
  // ordering did) throws std::bad_weak_ptr -- shared_from_this is the
  // documented reason this method moved out of the constructor.
  auto mapper = std::make_shared<LIVMapper>();
  // Order matters: loadFromRosNs uses shared_from_this(), so initializeCamera
  // must run AFTER make_shared returns but BEFORE any other component that
  // reads vio_manager->cam (vio_manager->initializeVIO at LIVMapper.cpp:260
  // calls cam->fx() etc., and that path runs from initializeComponents).
  mapper->initializeCamera();
  image_transport::ImageTransport it(mapper);
  mapper->initializeSubscribersAndPublishers(it);
  mapper->run();
  rclcpp::shutdown();
  return 0;
}
