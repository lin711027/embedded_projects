#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <filesystem>
#include <opencv2/opencv.hpp>
#include <onnxruntime_cxx_api.h>
#include "get_frames.h"
#include <thread>

namespace fs = std::filesystem;
std::string g_foldername;

int main(int argc, char** argv) {

    g_foldername = "frames_default";
    
    if (argc > 1) {
        
        g_foldername = argv[1];
        
    }
    std::cout << "g_foldername " << g_foldername << std::endl;
    std::thread t(run_camera_task);
    t.detach();
    while(1);
    return 0;
}