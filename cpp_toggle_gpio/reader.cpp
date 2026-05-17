#include "main.h"
int value;
void reader_task()
{
    while(running_recorder)
    {
        {
            std::lock_guard<std::mutex> lock(mtx);
            value=counter;
        }
        std::cout << "counter = " << value << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
    }

}
