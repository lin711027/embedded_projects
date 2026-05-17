#include <libcamera/libcamera.h>
#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <memory>
#include <chrono>
#include <thread>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <onnxruntime_cxx_api.h>
#include <filesystem>
#include "main.h"

using namespace libcamera;
namespace fs = std::filesystem;

std::shared_ptr<libcamera::Camera> g_camera;
std::vector<cv::Mat> frames;
std::mutex mtx;
std::condition_variable cv_batch;
std::queue<std::vector<cv::Mat>> batch_queue;
int frame_count = 0;
int width = 480;
int height = 640;
int infer_width=60;
int infer_height=80;

long long now_ms()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();    
}
static void requestComplete(libcamera::Request *request)
{
    if (request->status() == libcamera::Request::RequestCancelled){
        std::cout<< "camera is in idel"<<std::endl;
        return;
    }

    //pair.first   // key
    //pair.second  // value
    for (auto& pair: request->buffers())
    {
        libcamera::FrameBuffer *buffer = pair.second;
        const auto& metadata = buffer->metadata();
        
        
        size_t offset = 0;
        //把「裝置（camera buffer）」的資料，映射到你程式可以直接讀的記憶體, plane.fd.get() = file descriptor（檔案描述子）
        //camera buffer 在 kernel 空間
        //user space（你的程式）不能直接碰
        //👉 mmap 是橋樑
        //plane[0] = Y
        //plane[1] = U
        //plane[2] = V
        auto const &planes = buffer->planes();
        #if 0
        for (unsigned int i = 0; i < planes.size(); i++) {
            std::cout << "plane " << i
                << " fd=" << planes[i].fd.get()
                << " offset=" << planes[i].offset
                << " length=" << planes[i].length
                << std::endl;
        }
        #endif
        void *dataY = mmap(NULL, planes[0].length, PROT_READ, MAP_SHARED, planes[0].fd.get(), planes[0].offset);
        void *dataU = mmap(NULL, planes[1].length, PROT_READ, MAP_SHARED,planes[1].fd.get(), planes[1].offset);
        void *dataV = mmap(NULL, planes[2].length, PROT_READ, MAP_SHARED,planes[2].fd.get(), planes[2].offset);
        uint8_t *y = static_cast<uint8_t*>(dataY);
        uint8_t *v = static_cast<uint8_t*>(dataU);
        uint8_t *u = static_cast<uint8_t*>(dataV);
        int strideY = 512;   // 你前面印出來的 cfg.stride
        int strideU = 256;   // 通常是 strideY / 2
        int strideV = 256;
        std::vector<uint8_t> yuvData(640*480*3/2);
        // copy Y
        for (int row = 0; row < height; row++) {
            memcpy(yuvData.data() + row * width, y + row * strideY,width);
        }

        // copy U
        uint8_t *dstU = yuvData.data() + width * height;
        for (int row = 0; row < height / 2; row++) {
            memcpy( dstU + row * (width / 2), u + row * strideU, width / 2);
        }

        // copy V
        uint8_t *dstV = yuvData.data() + width * height + width * height / 4;
        for (int row = 0; row < height / 2; row++) {
            memcpy( dstV + row * (width / 2), v + row * strideV, width / 2 );
        
        }
        munmap(dataY, planes[0].length);
        munmap(dataU, planes[1].length);
        munmap(dataV, planes[2].length);

        cv::Mat yuv(height * 3 / 2, width, CV_8UC1, yuvData.data());        
        cv::Mat rgb;

        cv::cvtColor(yuv, rgb,cv::COLOR_YUV2RGB_I420);  
        cv::imwrite("frame.jpg", rgb);

        frames.push_back(rgb.clone());
        frame_count++;
        if(frame_count==30){
            std::cout << "[Camera] frame " << frame_count
                << " time = " << now_ms() << " ms"
                << std::endl;
            frame_count = 0; 
            std::vector<cv::Mat> batch;
            batch.swap(frames);

            {
                std::lock_guard<std::mutex> lock(mtx);
                batch_queue.push(std::move(batch));
            }
            cv_batch.notify_one();                    
        }
        request->reuse(libcamera::Request::ReuseBuffers);
        g_camera->queueRequest(request);        
    }
}

void recording_frames(cv::Mat img){
    static int frame_count;
    static bool recording = true;
    static bool folder_created = false;
    
    
    if(recording)
    {
        char filename[100];
        
        sprintf(filename, "%s/frame_%04d.jpg", g_foldername.c_str(),frame_count);

        std::cout << "recording frames g_foldername " << g_foldername << std::endl;
        if (!folder_created) {
            std::filesystem::create_directories(g_foldername);
            folder_created = true;
        }
       
        cv::imwrite(filename, img);
        frame_count++;

        if (frame_count == 120) {
            recording = false;
            std::cout << "Done 4 seconds capture" << std::endl;
        }        
    }
}

std::vector<float> preprocessBatch(const std::vector<cv::Mat> frames,int T, int C, int H, int W, 
                                   const float mean[3], const float stdv[3])
{
    std::vector<float> input_data(T*C*H*W);


    for(int t = 0;t<T; t++)
    {
        cv::Mat img =frames[t];
        cv::resize(img, img, cv::Size(W, H));
        
        recording_frames(img); //recording 120 frames image.
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
    return input_data;
}
void Inferencethread()
{

    const int T = 30;
    const int C = 3;
    const int H = 80;
    const int W = 60;
    const int num_classes = 23;
    const std::string model_path = "../stdconv_2221_ly34_NoQ-cnn_tcn_whole_260220.onnx";
    float mean[3] = {0.6378846f, 0.6091322f, 0.6168187f};
    float stdv[3] = {0.30198434f, 0.31222337f, 0.32536325f};

    while(1){
        std::vector<cv::Mat> batch;
        std::unique_lock<std::mutex> lock(mtx);

        cv_batch.wait(lock, []{
            return !batch_queue.empty();
        });
        batch = std::move(batch_queue.front());
        batch_queue.pop();
        auto t1 =now_ms();     

        if (batch.size() != T) {
            std::cerr << "Batch size error: " << batch.size() << std::endl;
            continue;
        }    


        std::vector<float> input_data =
            preprocessBatch(batch, T, C, H, W, mean, stdv);

        std::cout << "Preprocess done, input size = "
                  << input_data.size()
                  << std::endl;

        Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "onnx_cv_inference");
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
        // 這裡才跑 ONNX
        // run_model(batch);
        auto t2 =now_ms();
        std::cout << "Inference time = "<<  (t2 - t1) << " ms" << std::endl;
    }
}
void run_camera_task()
{
    CameraManager cm;
    std::cout << "starts get frames task" << std::endl;

    std::thread infer_thread(Inferencethread);
    //主程式不會等它結束
    //它會在背景一直跑
    //主程式結束時，它會被系統收掉
    infer_thread.detach();

    cm.start();
    if(cm.cameras().empty()){

        std:: cerr << "No Camera Found\n";
        return ;
    }
    g_camera = cm.cameras()[0];
    std::cout << g_camera << std::endl;
    std::cout << g_camera.get() << std::endl;

    int ret = g_camera->acquire();
    if(ret){
        std::cerr << "Failed to acquire g_camera" << std::endl;
    }
    // =====================
    // 設定 config
    // =====================
    std::unique_ptr<CameraConfiguration> config = g_camera->generateConfiguration({StreamRole::VideoRecording});

    config->at(0).size = {(unsigned int)width,(unsigned int)height};
    config->at(0).pixelFormat = formats::YUV420; // ✔ 穩定
    config->validate();
    std::cout << &config << std::endl;
    std::cout << config.get() << std::endl;
    g_camera->configure(config.get());
    
    Stream* stream = config->at(0).stream();
    std::cout << "camera has not started yet" << std::endl;
    std::cout << "size: " << config->at(0).size.width << "x" << config->at(0).size.height << std::endl;
    std::cout << "pixel format: " << config->at(0).pixelFormat.toString() << std::endl;
    std::cout << "stride: " << config->at(0).stride << std::endl;
    std::cout << "frameSize: " << config->at(0).frameSize << std::endl;
    // =====================
    // buffer allocator
    // =====================

    /*  camera 負責拍
        stream 決定怎麼拍
        allocator 準備記憶體
        request 告訴它「拍完放哪」*/
    FrameBufferAllocator allocator(g_camera);
    allocator.allocate(stream);

    std::vector<std::unique_ptr<Request>> requests;
    for(const std::unique_ptr<FrameBuffer> &buffer: allocator.buffers(stream))
    {
        
        std::unique_ptr<Request> request =g_camera->createRequest();
        
        request->addBuffer(stream, buffer.get());
        requests.push_back(std::move(request));

    }
    //callback
    g_camera->requestCompleted.connect(requestComplete);

    for (auto& p: requests){
        std::cout << p.get() <<std::endl;
    }

    libcamera::ControlList controls;

    controls.set(libcamera::controls::AeEnable, false);
    controls.set(libcamera::controls::ExposureTime, 20000); // 10 ms
    controls.set(libcamera::controls::AnalogueGain, 1.0f);    
    //controls.set(libcamera::controls::AwbEnable, false);  //關掉 AWB(改顏色、改亮度比例)
    //controls.set(libcamera::controls::ColourGains, {1.0f, 1.0f});
   // =====================
    // start camera
    // =====================
    g_camera->start(&controls);    
    //g_camera->start();    
    for(auto& req: requests){
        g_camera->queueRequest(req.get());

    }

    std::cout << "Camera started. Press Ctrl+C to stop." << std::endl;
    
    // =====================
    // 主 loop
    // =====================

    
    while(1){
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }    
    g_camera->stop();
    g_camera->release();
    cm.stop();

}


