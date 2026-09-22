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


# Previously pointed at /usr/local/include/simple-knn/ -- expecting
# simple-knn to have been built+installed system-wide separately, which
# nothing in this build actually does (unlike Livox-SDK2, which genuinely
# gets `make install`ed). Point directly at the vendored source instead --
# confirmed needed by hitting "spatial.h: No such file or directory" from
# lib3dgs/includes/gaussian.cuh, which pulls in simple-knn's headers
# directly (spatial.h, simple_knn.h), not via any installed package.
set(SIMPLE_KNN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../simple-knn")
include_directories(${SIMPLE_KNN_DIR})


set(LIBTORCH_DIR "./libtorch")
include_directories(${LIBTORCH_DIR}/include)
link_directories(${LIBTORCH_DIR}/lib)

# Previously "${LIBTORCH_DIR}/share/cmake/Torch" ("./libtorch", a manually
# downloaded-and-extracted LibTorch zip layout that was never actually
# populated by anything in this build). Point at the pixi/conda-forge
# `libtorch` package's real installed location instead -- $ENV{CONDA_PREFIX}
# keeps this portable across environments (Docker, a bare pod, local dev)
# rather than hardcoding one specific path.
set(Torch_DIR "$ENV{CONDA_PREFIX}/share/cmake/Torch")
find_package(Torch REQUIRED)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TORCH_CXX_FLAGS}")  # 添加Torch标志
include_directories(${TORCH_INCLUDE_DIRS})

find_package(PythonLibs REQUIRED)

# rpg_vikit: vendored sub-tree at src/rpg_vikit/ brings in the
# Robotic-Developer-Road/rpg_vikit fork (Dec 2025 ROS2 Jazzy port --
# the only known-working ROS2 source for the vikit_common/vikit_ros
# headers <vikit/abstract_camera.h>, <vikit/pinhole_camera.h>,
# <vikit/math_utils.h>, <vikit/robust_cost.h>, <vikit/vision.h> plus
# vk::camera_loader::loadFromRosNs(...) used at LIVMapper.cpp:219).
# The fork's top-level CMakeLists.txt (created by this vendoring) descends
# into src/rpg_vikit/vikit_common and src/rpg_vikit/vikit_ros, both patched
# to be pure ament_cmake packages (upstream's vikit_common had an
# IF(USE_ROS) catkin/plain-CMake fork with no ament support; vikit_ros
# already was ament_cmake but had spurious rosidl deps). Patches are
# contained inside the vendored tree -- no other file in this repo needs
# to know the details.
#
# Path fix: rpg_vikit is vendored as a SIBLING of this gs-livo/ directory
# (third_party/slam/gs_livo/src/{gs-livo,rpg_vikit}), not nested inside it
# -- fetch_slam_backends.py's sub-vendor convention places a "parent:child"
# dependency at third_party/slam/<parent>/src/<child>, alongside <parent>'s
# own src/<parent-name>, not underneath it. A bare add_subdirectory
# (rpg_vikit) looks for ${CMAKE_CURRENT_SOURCE_DIR}/rpg_vikit, which never
# exists -- confirmed by hitting exactly that ("add_subdirectory given
# source 'rpg_vikit' which is not an existing directory"). The explicit
# binary-dir argument is required because CMake's add_subdirectory()
# refuses an out-of-tree source directory (one that isn't inside
# CMAKE_CURRENT_SOURCE_DIR) without one.
add_subdirectory(../rpg_vikit ${CMAKE_CURRENT_BINARY_DIR}/rpg_vikit)

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

# Modern Sophus (conda-forge/pixi's package, target-based CMake config)
# only exports the Sophus::Sophus imported target and never sets the
# legacy Sophus_INCLUDE_DIRS variable this file uses below via a bare
# include_directories() call -- confirmed by hitting "fatal error:
# sophus/se3.h: No such file or directory" in every target compiled after
# that call (imu_proc, vio, etc.) despite find_package(Sophus REQUIRED)
# succeeding. Recover it from the target, same pattern already applied to
# every other Cerberus SLAM fork that hit this (orb_slam3, multicam_slam,
# fast_lio2's g2o).
if(NOT Sophus_INCLUDE_DIRS AND TARGET Sophus::Sophus)
  get_target_property(Sophus_INCLUDE_DIRS Sophus::Sophus INTERFACE_INCLUDE_DIRECTORIES)
endif()

# Sophus is header-only in every modern release (including the
# pixi/conda-forge package this build uses) -- there has never been a real
# libSophus.so/.a to link. This hardcoded bare filename relied on the
# linker's default search path silently finding SOME "libSophus.so"
# (presumably true on whatever machine originally authored this file, from
# an old Sophus release that did ship a compiled library); on a clean
# environment it just fails with "cannot find -lSophus" -- confirmed by
# hitting exactly that. Leave Sophus_LIBRARIES empty; find_package(Sophus)
# above already gives this file everything it actually needs via headers.
set(Sophus_LIBRARIES "")

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
  # find_package(PythonLibs REQUIRED) above populates PYTHON_INCLUDE_DIRS,
  # but nothing ever actually added it to an include path -- LibTorch's own
  # torch/csrc/python_headers.h does #include <Python.h> unconditionally,
  # so anything that pulls in a LibTorch header transitively (as
  # LIVMapper.cpp does) fails with "Python.h: No such file or directory"
  # without this. Confirmed by hitting exactly that.
  ${PYTHON_INCLUDE_DIRS}
  include
  # GSVoxelOctree.h (included transitively by multiple targets here, not
  # just vio -- e.g. lio/imu_proc/laser_mapping via common_lib.h and
  # sibling headers) does `#include "debug_utils.cuh"`, which only lives
  # under lib3dgs/includes/. Previously only vio's own
  # target_include_directories() covered this path, so every OTHER target
  # compiling a translation unit that transitively includes
  # GSVoxelOctree.h failed with "debug_utils.cuh: No such file or
  # directory" -- confirmed by hitting exactly this. Promote these to the
  # shared directory scope so every target gets them uniformly.
  ${CMAKE_CURRENT_SOURCE_DIR}/../lib3dgs/includes
  ${CMAKE_CURRENT_SOURCE_DIR}/../lib3dgs/cuda_rasterizer
  ${CMAKE_CURRENT_SOURCE_DIR}/../lib3dgs/external/tinyply/source
  # Same class of gap as lib3dgs above: frame.h (#include
  # <vikit/abstract_camera.h>) is transitively included by laser_mapping's
  # LIVMapper.cpp, but only `vio` actually links against vikit_common/
  # vikit_ros -- CMake's transitive INTERFACE_INCLUDE_DIRECTORIES
  # propagation only reaches targets that directly (or transitively via
  # their OWN target_link_libraries) link the exporting target, and
  # laser_mapping doesn't. Confirmed by hitting "vikit/abstract_camera.h:
  # No such file or directory" compiling LIVMapper.cpp. Add both include
  # roots to the shared scope so every target gets them uniformly,
  # regardless of which targets end up linking vikit_common/vikit_ros.
  ${CMAKE_CURRENT_SOURCE_DIR}/../rpg_vikit/vikit_common/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../rpg_vikit/vikit_ros/include
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

# vikit_common/vikit_ros deliberately excluded from this list --
# ament_target_dependencies() resolves entries via find_package()-populated
# variables/imported targets, which the in-tree add_subdirectory(rpg_vikit)
# case never populates (confirmed: "ament_target_dependencies() the passed
# package name 'vikit_common' was not found before" -- same root cause as
# the identical fix applied to vikit_ros's own CMakeLists.txt, see
# docker/slam/gs_livo/patches/rpg_vikit/vikit_ros/CMakeLists.txt). Linked
# directly below instead.
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

# vikit headers are referenced via PUBLIC include dirs from vikit_common /
# vikit_ros (set up by the add_subdirectory(rpg_vikit) call above). vio.cpp
# / vio.h includes <vikit/math_utils.h>, <vikit/robust_cost.h>,
# <vikit/vision.h>, <vikit/pinhole_camera.h>; frame.h includes
# <vikit/abstract_camera.h>; LIVMapper.h includes <vikit/camera_loader.h>.
# None of these headers live inside gs-livo itself -- they all come from
# the vendored rpg_vikit/vikit_common sub-tree. Link plain, unnamespaced
# `vikit_common`/`vikit_ros` -- the patched rpg_vikit CMakeLists.txt
# (docker/slam/gs_livo/patches/rpg_vikit/) export their targets without a
# NAMESPACE, so the in-tree add_subdirectory() target names ARE
# `vikit_common`/`vikit_ros`, not `vikit_common::vikit_common`/
# `vikit_ros::vikit_ros` -- confirmed by hitting the namespaced form
# failing to resolve. vikit_ros brings vk::camera_loader::loadFromRosNs.
target_link_libraries(vio
  ${LIB3DGS_LIB_PATH}
  vikit_common
  vikit_ros
  # ${TORCH_LIBRARIES}
  # tinyply
  # simple-knn
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
# PYTHON_LIBRARIES: pybind11 code pulled in transitively via LibTorch's own
# headers references CPython C-API symbols (_Py_Dealloc, PyTuple_New,
# PyErr_Occurred, etc.) directly -- confirmed by hitting undefined
# references to exactly those at final link time. find_package(PythonLibs)
# earlier in this file already resolves this variable; it was just never
# actually linked (only ${PYTHON_INCLUDE_DIRS} was wired in, for Python.h).
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
  ${PYTHON_LIBRARIES}
)

# vikit_common/vikit_ros excluded here too -- same reason as vio's list
# above; fastlivo_mapping already picks them up transitively by linking
# against `vio` (see target_link_libraries above).
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

# ament_package() is intentionally NOT called here. The vendored rpg_vikit/
# vikit_common and rpg_vikit/vikit_ros sub-trees each call ament_package()
# themselves (see src/rpg_vikit/{vikit_common,vikit_ros}/CMakeLists.txt),
# and ament_cmake forbids more than one ament_package() call per CMake
# configure run -- calling it again here produces an "ament_package() called
# more than once" error at configure time. The documented ament idiom for
# "this CMakeLists.txt is plain CMake with ament targets, but is not itself
# an ament package" is exactly this: build targets, install them via
# install(TARGETS ...) / install(DIRECTORY ...), but skip the final
# ament_package() call. The matching <export> block in package.xml has been
# removed for the same reason (an ament_cmake manifest with build_type but
# no matching ament_package() call in CMakeLists.txt is invalid).
