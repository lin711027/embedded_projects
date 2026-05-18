#pragma once
#include <iostream>
#include <thread>
#ifdef TARGETBOARD_IMX8MM
#include <dirent.h>
#else
#include <filesystem>
namespace fs = std::filesystem;
#endif
#include <vector>
#include <string>
#include <mutex>
#include <condition_variable>
#include <opencv2/opencv.hpp>
#include <onnxruntime_cxx_api.h>
