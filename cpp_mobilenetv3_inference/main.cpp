#include "main.h"
#include <mutex>
#include <condition_variable>
namespace fs = std::filesystem;
std::mutex mtx;
std::condition_variable cvbatch;
std::string folder_path = "../samples/Biking_g01_c01";
    const int T = 30;
    const int C = 3;
    const int H = 240;
    const int W = 320;
    const int num_classes = 10;
std::vector<float> input_data(1 * T * C * H * W); //真正的影像資料記憶體    
void read_img_task()
{
    while (1){
        for(int t=0;t<T;t++)
    {
        cv::Mat img = cv::imread(files[t], cv::IMREAD_COLOR);
        if(img.empty()){
            std::cerr << "Failed to read captured frame " << std::endl;
            return -1;
        }
        cv::cvtColor(img, img, cv::COLOR_BGR2RGB);
        cv::resize(img, img, cv::Size(W,H));
        img.convertTo(img, CV_32FC3, 1.0/255.0);

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
    {
        std::lock_guard<std::mutex> lock(mtx);
        
    }
    cvbatch.notify_one();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
void inference_task()
{
   
    while(1)
    {
        std::unique_lock<std::mutex> lock(mtx);
        cvbatch.wait(lock, []{
            return !input_data.empty();
        });
    Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "mobilenetv3_inference");
    Ort::SessionOptions session_options;
    session_options.SetIntraOpNumThreads(1);

    Ort::Session session(env, model_path.c_str(), session_options);

    Ort::AllocatorWithDefaultOptions allocator;

    auto input_name_ptr = session.GetInputNameAllocated(0, allocator);
    auto output_name_ptr = session.GetOutputNameAllocated(0, allocator);

    const char* input_name = input_name_ptr.get();
    const char* output_name = output_name_ptr.get();

    std::cout << input_name << std::endl;
    std::cout << output_name << std::endl;

    std::vector<int64_t> input_sharp = {1,T,C,H,W}; //告訴 ONNX tensor 長怎樣, 只有5個 int_64

    Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(
        OrtArenaAllocator,
        OrtMemTypeDefault
    );
    Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
        memory_info, 
        input_data.data(),
        input_data.size(),
        input_sharp.data(),
        input_sharp.size()
    );
    std::vector<const char*> input_names = {input_name};
    std::vector<const char*> output_names = {output_name};

    std::cout << input_names[0] << " : " << std::endl;

    auto output_tensor = session.Run(
        Ort::RunOptions{nullptr},
        input_names.data(),
        &input_tensor,
        1,
        output_names.data(),
        1
    );
    float *logist = output_tensor[0].GetTensorMutableData<float>();

    for(int i=0;i<num_classes;i++){
        std::cout << logist[i] << std::endl;
    }
    int prev = 0;

    float max_val = logist[0];

    for(int i = 1; i < num_classes ; i++)
    {
        if(logist[i] > max_val)
        {
            max_val=logist[i];
            prev = i;
        }
    }
    std::cout << "Logist:" << std::endl;
    for(int i = 0 ; i < num_classes; i++)
    {
        std::cout << logist[i] << " ";
    }
    std::cout << std::endl;
    std::cout << "Pred class index: " << prev << std::endl;
    }
}
int main(int argc, char** argv)
{
    const std::string model_path = "../MobileNetv3-large-last1-smot01-TCN-4l-0.4D_2221_lyNoGN_NoQ-cnn_tcn_whole_260509.onnx";
    //const std::string folder_path = "../samples/Biking_g01_c01";

    float mean[3] = {0.485f, 0.456f, 0.406f};
    float stdv[3] = {0.229f, 0.224f, 0.225f};  
    
    if(argc > 1)
    {
        std::cout << argv[1] << std::endl;
        if(std::string(argv[1])=="1")
            folder_path="../samples/Biking_g01_c01";
        else if(std::string(argv[1])=="2")
            folder_path="../samples/Basketball_g01_c05";
        else if(std::string(argv[1])=="3")
            folder_path="../samples/Diving_g01_c01";
        else if(std::string(argv[1])=="4")
            folder_path="../samples/FloorGymnastics_g01_c02";        
        else 
            folder_path="../samples/Biking_g01_c01";
    }
    std::cout << folder_path.c_str() << std::endl;
    std::vector<std::string> files;
    for(auto & p : fs::directory_iterator(folder_path))
    {
        if(p.path().extension() == ".jpg")
        {
            files.push_back(p.path().string());
            //std::cout << p.path().string() << std::endl;
        }
    }
    std::sort(files.begin(), files.end());

    if(files.size()<T){
        std::cerr << "Not enough frames." << std::endl;
        return -1;
    }
    std::thread read_img_thread(read_img_task);
    std::thread inference_thread(inference_task);
    whiel(1);
    read_img_thread.join();
    inference_thread.join();
    return 0;

}