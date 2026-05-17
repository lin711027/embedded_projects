#include "main.h"

namespace fs = std::filesystem;

void gpio_task(void);
void reader_task(void);
void writer_task(void);
std::atomic<bool> runnung, running_recorder;
std::mutex mtx;
int counter;
int main()
{
    runnung=true;
    running_recorder=true;
    std::thread gpio_thread(gpio_task);
    std::thread writer_thread(writer_task);
    std::thread reader_thread(reader_task);

    gpiod_chip *chip = gpiod_chip_open("/dev/gpiochip0");
    std::cout << chip << std::endl;

    if(!chip){

        std::cerr << "Failed to open gpiochip0\n";
        return -1;        
    }
    unsigned int offsets[] = {17};
    
    gpiod_line_settings *settings = gpiod_line_settings_new();
    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);
    gpiod_line_settings_set_output_value(settings, GPIOD_LINE_VALUE_INACTIVE);

    gpiod_line_config *line_cfg = gpiod_line_config_new();
    gpiod_line_config_add_line_settings(line_cfg, offsets,1,settings);

    gpiod_request_config  *req_cfg = gpiod_request_config_new();

    gpiod_line_request  *request = gpiod_chip_request_lines(chip, req_cfg, line_cfg);

    if(!request){
        std::cerr << "Failed to request line\n";
        return -1;
    }

    int value = 0;

    while(true){

        value = !value;
        gpiod_line_request_set_value(request, 17, value ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE);
        
        std::cout  << "GPIO17 = " << value << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    runnung=false;
    running_recorder=false;
    gpio_thread.join();
    std::cout << "close gpio_thread" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    reader_thread.join();
    std::cout << "close reader_thread" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    writer_thread.join();
    std::cout << "close writer_thread" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));    
    gpiod_line_request_release(request);
    gpiod_chip_close(chip);

    return 0;
}