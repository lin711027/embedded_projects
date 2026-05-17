cmake_minimum_required(VERSION 3.10)

set(ONNXRUNTIME_DIR "/home/jackylin/onnxruntime-linux-aarch64-1.17.3")
if(COMMAND cmake_policy)
    cmake_policy(SET CMP0003 NEW)
endif()
project(onnx_cpp_test)
set(CMAKE_CXX_STANDARD)
find_package(OpenCV REQUIRED)
include_directories(
    ${OpenCV_INCLUDE_DIRS}
    ${ONNXRUNTIME_DIR}/include
)
link_directories(
    ${ONNXRUNTIME_DIR}/lib
)

add_executable(onnx_test main.cpp)
target_link_libraries(onnx_test ${OpenCV_LIBS} onnxruntime)
