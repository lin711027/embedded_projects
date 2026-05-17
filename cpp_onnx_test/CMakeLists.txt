cmake_minimum_required(VERSION 3.10)
project(onnx_cpp_test)
set(CMAKE_CXX_STANDARD 17)
message(STATUS "CMAKE_SYSTEM_PROCESSOR = ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "CMAKE_CXX_COMPILER =${CMAKE_CXX_COMPILER}")

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    set(TARGET_SYSROOT $ENV{SDKTARGETSYSROOT})
    message(STATUS "TARGET_SYSROOT = ${TARGET_SYSROOT}")
    set(ONNXRUNTIME_DIR "/root/onnxruntime-linux-aarch64-1.17.3")
    set(IS_TARGET_BOARD TRUE)
    set(OPENCV_INCLUDE_DIR ${TARGET_SYSROOT}/usr/include/opencv)
    set(OPENCV_LIBS
        ${TARGET_SYSROOT}/usr/lib/libopencv_core.so
        ${TARGET_SYSROOT}/usr/lib/libopencv_imgproc.so
        ${TARGET_SYSROOT}/usr/lib/libopencv_imgcodecs.so)
else()
    set(ONNXRUNTIME_DIR "/root/onnxruntime-linux-x64-1.17.3")
    set(IS_TARGET_BOARD FALSE)
    find_package(OpenCV REQUIRED)
    set(OPENCV_INCLUDE_DIR ${OpenCV_INCLUDE_DIRS})
    set(OPENCV_LIBS ${OpenCV_LIBS})
endif()

include_directories(
    ${OPENCV_INCLUDE_DIR}
    ${ONNXRUNTIME_DIR}/include
)
link_directories( onnx_test  PRIVATE
    ${ONNXRUNTIME_DIR}/lib
)

add_executable(onnx_test main.cpp)
if(IS_TARGET_BOARD) 
    target_compile_definitions(onnx_test PRIVATE TARGETBOARD_IMX8MM)
endif()

target_link_libraries(onnx_test PRIVATE
        ${OPENCV_LIBS} 
        onnxruntime)
