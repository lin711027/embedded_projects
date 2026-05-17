cmake_minimum_required(VERSION 3.10)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBCAMERA REQUIRED libcamera)

set(ONNXRUNTIME_DIR "/home/jackylin/onnxruntime-linux-aarch64-1.17.3")
if(COMMAND cmake_policy)
    cmake_policy(SET CMP0003 NEW)
endif()
project(onnx_cpp_test)
set(CMAKE_CXX_STANDARD 17 )
find_package(OpenCV REQUIRED)
include_directories(
    ${OpenCV_INCLUDE_DIRS}
    ${ONNXRUNTIME_DIR}/include
)
link_directories(
    ${ONNXRUNTIME_DIR}/lib
)

add_executable(onnx_cv_inference main.cpp get_frames.cpp)
target_link_libraries(onnx_cv_inference PRIVATE ${OpenCV_LIBS} ${LIBCAMERA_LIBRARIES} onnxruntime)
target_include_directories(onnx_cv_inference PRIVATE ${LIBCAMERA_INCLUDE_DIRS})
target_compile_options(onnx_cv_inference PRIVATE ${LIBCAMERA_CFLAGS_OTHER})