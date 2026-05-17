#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <filesystem>
#include <opencv2/opencv.hpp>
#include <onnxruntime_cxx_api.h>

namespace fs = std::filesystem;

int main() {
    const std::string model_path = "../stdconv_2221_ly34_NoQ-cnn_tcn_whole_260220.onnx";
    const std::string folder_path = "../sample";

    const int T = 30;
    const int C = 3;
    const int H = 80;
    const int W = 60;
    const int num_classes = 23;

    float mean[3] = {0.6378846f, 0.6091322f, 0.6168187f};
    float stdv[3] = {0.30198434f, 0.31222337f, 0.32536325f};
#if 0
    std::vector<std::string> files;
    for (auto &p : fs::directory_iterator(folder_path)) {
        if (p.path().extension() == ".bmp") {
            files.push_back(p.path().string());
        }
    }

    std::sort(files.begin(), files.end());

    for (const auto& f : files)
        std::cout<<f<<std::endl;

    if (files.size() < T) {
        std::cerr << "Not enough frames." << std::endl;
        return -1;
    }
#endif

    // =========================
    // 1. 開啟 camera
    // =========================
    #if 0
    cv::VideoCapture cap(    
        "rpicam-vid --inline --width 480 --height 640 --codec mjpeg -o -",
        cv::CAP_FFMPEG
    );

    if (!cap.isOpened()) {
        std::cerr << "Failed to open camera." << std::endl;
        std::cerr << "Try: ls /dev/video*" << std::endl;
        return -1;
    }

    cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    cap.set(cv::CAP_PROP_FPS, 30);

    std::cout << "Camera opened." << std::endl;
 #endif   
    // =========================
    // 2. 建立 input tensor data
    // shape = [1, T, C, H, W]
    // =========================    
    std::vector<float> input_data(1*T*C*H*W);

    for (int t = 0; t < T; t++) {
    std::string cmd =
        "rpicam-still -n --timeout 1 "
        "--width 480 --height 640 "
        "-o /tmp/frame.jpg"
        "> /dev/null 2>&1";

    int ret = system(cmd.c_str());
    if (ret != 0) {
        std::cerr << "rpicam-still failed at frame " << t << std::endl;
        return -1;
    }

    cv::Mat img = cv::imread("/tmp/frame.jpg", cv::IMREAD_COLOR);

    if (img.empty()) {
        std::cerr << "Failed to read captured frame " << t << std::endl;
        return -1;
    }

        cv::cvtColor(img, img, cv::COLOR_BGR2RGB);
        cv::resize(img, img, cv::Size(W, H));
        img.convertTo(img, CV_32FC3, 1.0 / 255.0);

        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                cv::Vec3f pixel = img.at<cv::Vec3f>(y, x);

                for (int c = 0; c < C; c++) {
                    float value = (pixel[c] - mean[c]) / stdv[c];

                    int index =
                        t * C * H * W +
                        c * H * W +
                        y * W +
                        x;

                    input_data[index] = value;
                }
            }
        }
    }

    Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "onnx_test");
    Ort::SessionOptions session_options;
    session_options.SetIntraOpNumThreads(1);

    Ort::Session session(env, model_path.c_str(), session_options);

    Ort::AllocatorWithDefaultOptions allocator;

    auto input_name_ptr = session.GetInputNameAllocated(0, allocator);
    auto output_name_ptr = session.GetOutputNameAllocated(0, allocator);

    const char* input_name = input_name_ptr.get();
    const char* output_name = output_name_ptr.get();

    std::vector<int64_t> input_shape = {1, T, C, H, W};

    Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(
        OrtArenaAllocator,
        OrtMemTypeDefault
    );

    Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
        memory_info,
        input_data.data(),
        input_data.size(),
        input_shape.data(),
        input_shape.size()
    );

    std::vector<const char*> input_names = {input_name};
    std::vector<const char*> output_names = {output_name};

    auto output_tensors = session.Run(
        Ort::RunOptions{nullptr},
        input_names.data(),
        &input_tensor,
        1,
        output_names.data(),
        1
    );

    float* logits = output_tensors[0].GetTensorMutableData<float>();

    int pred = 0;
    float max_val = logits[0];

    for (int i = 1; i < num_classes; i++) {
        if (logits[i] > max_val) {
            max_val = logits[i];
            pred = i;
        }
    }

    std::cout << "Logits:" << std::endl;
    for (int i = 0; i < num_classes; i++) {
        std::cout << logits[i] << " ";
    }

    std::cout << std::endl;
    std::cout << "Pred class index: " << pred << std::endl;

    return 0;
}