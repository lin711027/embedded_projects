#include "main.h"

void gpio_task(void)
{
    gpiod_chip* chip = gpiod_chip_open("/dev/gpiochip0");
    std::cout << "gpio_thread :" << chip << std::endl;
    if(!chip){
        std::cerr << "Failed to open gpiochip0\n";
        return ;        
    }
    unsigned int offset[] = {2};
    gpiod_line_settings *settings = gpiod_line_settings_new();
    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);
    gpiod_line_settings_set_output_value(settings, GPIOD_LINE_VALUE_INACTIVE);

    gpiod_line_config *line_cfg = gpiod_line_config_new();
    gpiod_line_config_add_line_settings(line_cfg, offset, 1, settings);

    gpiod_request_config *req_cfg = gpiod_request_config_new();

    gpiod_line_request *request =gpiod_chip_request_lines(chip, req_cfg, line_cfg);

    if(!request){
        std::cerr << "Failed to request line\n";
        return ;       
    }
    int value=0;
    while(runnung)
    {
        value = !value;
        gpiod_line_request_set_value(request, 2, value ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE);
        std::cout << "GPIO2 :" << value << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    gpiod_line_request_release(request);
    gpiod_chip_close(chip);

}