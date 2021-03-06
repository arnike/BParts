cmake_minimum_required(VERSION 2.8.3)

project(POSE_ESTIMATION)

#find_package(PCL 1.3 REQUIRED COMPONENTS common io visualization search segmentation features filters surface kdtree)

################################
# OpticalFlow Library
################################
find_library(LDOF ldof PATHS tracking)

set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
         
set(SRC_FILES src/keyframe.cpp
			  src/data_io_backend.cpp
			  src/data_io.cpp
              src/stanford/backend.cpp
              src/bonn/backend.cpp
              src/main.cpp
              src/classifier_svm.cpp
              src/timer.cpp
              src/opflow.cpp
              src/opflow_tensor.cpp
              )

set(MESH_FILES src/mesher.cpp src/timer.cpp)

set(LIBRARIES opencv_core
		opencv_highgui
		boost_system
		boost_filesystem
		opencv_imgproc
		gtest
		svm
		${LDOF})

include_directories(include
	${CMAKE_INCLUDE_PATH}
	libsvm-3.17
	tracking
)

LINK_DIRECTORIES(${CMAKE_BINARY_DIR}/lib)

add_library(mesh ${MESH_FILES})
target_link_libraries(mesh ${LIBRARIES})

add_executable(estimate_pose ${SRC_FILES})
add_executable(extract_keypoints src/tools/extract_keypoints.cpp
									src/data_io.cpp
              						src/stanford/backend.cpp
              						src/bonn/backend.cpp
              						src/data_io_backend.cpp
              						src/keyframe.cpp)

target_link_libraries(estimate_pose mesh ${LIBRARIES})
target_link_libraries(extract_keypoints ${LIBRARIES})

################################
# LibSVM
################################
ADD_SUBDIRECTORY (libsvm-3.17)

################################
# GTest
################################
ADD_SUBDIRECTORY (gtest-1.7.0)
enable_testing()
include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})

################################
# Unit Tests
################################
# Add test cpp file
add_executable( runUnitTests tests/mesh_tests.cpp tests/dataset_tests.cpp )
# Link test executable against gtest & gtest_main
target_link_libraries(runUnitTests mesh gtest gtest_main )
add_test( runUnitTests runUnitTests )
