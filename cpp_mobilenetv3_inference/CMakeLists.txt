cmake_minimum_required(VERSION 3.10)
project(cpp_mobilenetv3_inference)
set(CMAKE_CXX_STANDARD 17)

find_package(OpenCV REQUIRED)
set(ONNXRUNTIME_DIR "/home/jackylin/onnxruntime-linux-aarch64-1.17.3")
include_directories(
    ${OpenCV_INCLUDE_DIRS}
    ${ONNXRUNTIME_DIR}/include
)
link_directories(${ONNXRUNTIME_DIR}/bin)
add_executable(mobilenetv3_inference main.cpp)
target_link_libraries(mobilenetv3_inference ${OpenCV_LIBS} ${ONNXRUNTIME_DIR}/lib/libonnxruntime.so)