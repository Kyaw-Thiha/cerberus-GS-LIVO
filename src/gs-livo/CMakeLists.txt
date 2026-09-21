cmake_minimum_required(VERSION 3.16)
project(fast_livo CUDA CXX)

find_package(CUDA REQUIRED)
if(NOT CUDA_FOUND)
  message(FATAL_ERROR "CUDA not found! Install CUDA Toolkit.")
endif()
include_directories(${CUDA_INCLUDE_DIRS})
message(STATUS "CUDA_INCLUDE_DIRS: ${CUDA_INCLUDE_DIRS}")

set(CMAKE_BUILD_TYPE "Release")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")

# set(CMAKE_CUDA_COMPILER /usr/local/cuda-11.8/bin/nvcc)

set(PYTHON_EXECUTABLE /usr/bin/python3)
find_package(PythonLibs REQUIRED)

set(LIBTORCH_DIR "./libtorch")
set(LIBTORCH_INCLUDE_DIR "${LIBTORCH_DIR}/include/torch/csrc/api/include")


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set common compile options
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -fexceptions")

# Specific settings for Debug build
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g")

# Detect CPU architecture
message(STATUS "Current CPU architecture: ${CMAKE_SYSTEM_PROCESSOR}")

# Specific settings for Release build
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm|aarch64|ARM|AARCH64)")
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    # 64-bit ARM optimizations (e.g., RK3588 and Jetson Orin NX)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -mcpu=native -mtune=native -ffast-math")
    message(STATUS "Using 64-bit ARM optimizations: -O3 -mcpu=native -mtune=native -ffast-math")
  else()
    # 32-bit ARM optimizations with NEON support
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -mcpu=native -mtune=native -mfpu=neon -ffast-math")
    message(STATUS "Using 32-bit ARM optimizations: -O3 -mcpu=native -mtune=native -mfpu=neon -ffast-math")
  endif()
  add_definitions(-DARM_ARCH)
else()
  # x86-64 (Intel/AMD) optimizations
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native -mtune=native -funroll-loops") #-flto
  message(STATUS "Using general x86 optimizations: -O3 -march=native -mtune=native -funroll-loops")
  add_definitions(-DX86_ARCH)
endif()

# Define project root directory
add_definitions(-DROOT_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/\")

# Detect CPU core count for potential multithreading optimization
include(ProcessorCount)
ProcessorCount(N)
message(STATUS "Processor count: ${N}")

# Set the number of cores for multithreading
if(N GREATER 4)
  math(EXPR PROC_NUM "4")
  add_definitions(-DMP_EN -DMP_PROC_NUM=${PROC_NUM})
  message(STATUS "Multithreading enabled. Cores: ${PROC_NUM}")
elseif(N GREATER 1)
  math(EXPR PROC_NUM "${N}")
  add_definitions(-DMP_EN -DMP_PROC_NUM=${PROC_NUM})
  message(STATUS "Multithreading enabled. Cores: ${PROC_NUM}")
else()
  add_definitions(-DMP_PROC_NUM=1)
  message(STATUS "Single core detected. Multithreading disabled.")
endif()

set(PYTHON_EXECUTABLE /usr/bin/python3)


set(SIMPLE_KNN_DIR "/usr/local")
include_directories(${SIMPLE_KNN_DIR}/include/simple-knn/)
link_directories(${SIMPLE_KNN_DIR}/lib)


set(LIBTORCH_DIR "./libtorch")
include_directories(${LIBTORCH_DIR}/include)
link_directories(${LIBTORCH_DIR}/lib)

set(Torch_DIR "${LIBTORCH_DIR}/share/cmake/Torch")
find_package(Torch REQUIRED)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TORCH_CXX_FLAGS}")  # 添加Torch标志
include_directories(${TORCH_INCLUDE_DIRS})

find_package(PythonLibs REQUIRED)

# rpg_vikit (src/rpg_vikit) is expected to be a vendored/submodule directory
# providing vikit_common/vikit_ros-equivalent helpers. It is currently EMPTY
# in this checkout (no CMakeLists.txt) -- a pre-existing gap unrelated to
# the ROS1->ROS2 migration -- so this add_subdirectory call will fail to
# configure until it is properly vendored. Left as-is (translated verbatim,
# not newly broken) pending that separate fix.
add_subdirectory(rpg_vikit)

# vikit_common / vikit_ros have no ROS2 (ament) release and are not
# system-installed here; they were only ever provided by the vendored
# rpg_vikit subdirectory above, which is itself empty in this checkout.
# Converted to ament-style find_package calls (catkin's
# `COMPONENTS vikit_common vikit_ros` has no direct ament equivalent) so
# the dependency is still declared, but this will fail to resolve until
# rpg_vikit is properly vendored -- a known pre-existing gap, not
# something fixed by this ROS1->ROS2 build-config migration.
find_package(vikit_common QUIET)
find_package(vikit_ros QUIET)
# Check for OpenMP support
find_package(OpenMP QUIET)
if(OpenMP_CXX_FOUND)
  message(STATUS "OpenMP found")
  add_compile_options(${OpenMP_CXX_FLAGS})
else()
  message(STATUS "OpenMP not found, proceeding without it")
endif()

# Check for mimalloc support
find_package(mimalloc QUIET)
if(mimalloc_FOUND)
  message(STATUS "mimalloc found")
else()
  message(STATUS "mimalloc not found, proceeding without it")
endif()

# Find ament_cmake and required ROS2 package dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(visualization_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(tf2 REQUIRED)
find_package(tf2_ros REQUIRED)
find_package(tf2_geometry_msgs REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(image_transport REQUIRED)
find_package(pcl_conversions REQUIRED)
find_package(PCL REQUIRED)
# livox_ros_driver2 is the ROS2 equivalent of the vendored ROS1
# livox_ros_driver headers under include/livox_ros_driver/. It is declared
# here as an ament dependency; whether the vendored ROS1 header is deleted
# and its #includes repointed at <livox_ros_driver2/msg/custom_msg.hpp> is
# left to the agent porting the C++ call sites (out of scope for this
# build-config-only change).
find_package(livox_ros_driver2 REQUIRED)

find_package(Eigen3 REQUIRED)
find_package(OpenCV REQUIRED)
find_package(Sophus REQUIRED)
find_package(Boost REQUIRED COMPONENTS thread)

set(Sophus_LIBRARIES libSophus.so)

# NOTE: EIGEN3_INCLUDE_DIR / PCL_INCLUDE_DIRS below are legacy
# variable-based (non target-based) usage carried over unchanged from the
# catkin build. A fuller CMake-target-based cleanup (Eigen3::Eigen,
# PCL target usage, etc.) is future work -- left alone here to keep this
# change scoped to the catkin -> ament_cmake build-system migration.
include_directories(
  ${EIGEN3_INCLUDE_DIR}
  ${PCL_INCLUDE_DIRS}
  ${OpenCV_INCLUDE_DIRS}
  ${Sophus_INCLUDE_DIRS}
  include
)

# Path to the built lib3dgs shared library. Defaults to the sibling
# lib3dgs/build directory in this repo, but can be overridden at configure
# time, e.g.:
#   cmake -DLIB3DGS_LIB_PATH=/path/to/lib3dgs_lib.so ..
set(LIB3DGS_LIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../lib3dgs/build/lib3dgs_lib.so"
  CACHE FILEPATH "Path to the built lib3dgs_lib shared library")

# Add libraries
add_library(vio src/vio.cpp src/frame.cpp src/visual_point.cpp)

target_link_libraries(vio
  ${LIB3DGS_LIB_PATH}
  # ${TORCH_LIBRARIES}
  # tinyply
  # simple-knn
)

target_include_directories(vio PUBLIC
${CMAKE_CURRENT_SOURCE_DIR}/../lib3dgs/includes
${CMAKE_CURRENT_SOURCE_DIR}/../lib3dgs/cuda_rasterizer
${CMAKE_CURRENT_SOURCE_DIR}/../lib3dgs/external/tinyply/source
${LIBTORCH_INCLUDE_DIR}
)

ament_target_dependencies(vio
  rclcpp
  sensor_msgs
  nav_msgs
  geometry_msgs
  visualization_msgs
  std_msgs
  tf2
  tf2_ros
  tf2_geometry_msgs
  cv_bridge
  image_transport
  pcl_conversions
  livox_ros_driver2
)

add_library(lio src/voxel_map.cpp)
add_library(pre src/preprocess.cpp)
add_library(imu_proc src/IMU_Processing.cpp)
add_library(laser_mapping src/LIVMapper.cpp)

foreach(_gslivo_lib lio pre imu_proc laser_mapping)
  ament_target_dependencies(${_gslivo_lib}
    rclcpp
    sensor_msgs
    nav_msgs
    geometry_msgs
    visualization_msgs
    std_msgs
    tf2
    tf2_ros
    tf2_geometry_msgs
    cv_bridge
    image_transport
    pcl_conversions
    livox_ros_driver2
  )
endforeach()

# Add the main executable
add_executable(fastlivo_mapping src/main.cpp)

# Link libraries to the executable
target_link_libraries(fastlivo_mapping
  laser_mapping
  vio
  lio
  pre
  imu_proc
  ${PCL_LIBRARIES}
  ${OpenCV_LIBRARIES}
  ${Sophus_LIBRARIES}
  ${Boost_LIBRARIES}
  ${TORCH_LIBRARIES}  # 添加
)

ament_target_dependencies(fastlivo_mapping
  rclcpp
  sensor_msgs
  nav_msgs
  geometry_msgs
  visualization_msgs
  std_msgs
  tf2
  tf2_ros
  tf2_geometry_msgs
  cv_bridge
  image_transport
  pcl_conversions
  livox_ros_driver2
)

# Link mimalloc if found
if(mimalloc_FOUND)
  target_link_libraries(fastlivo_mapping mimalloc)
endif()

install(TARGETS fastlivo_mapping
  DESTINATION lib/${PROJECT_NAME})

install(DIRECTORY launch config rviz_cfg
  DESTINATION share/${PROJECT_NAME})

ament_package()
