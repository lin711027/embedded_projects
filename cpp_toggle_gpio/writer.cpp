#include "main.h"
void writer_task()
{
    while(running_recorder)
    {

        {
            std::lock_guard<std::mutex> lock(mtx);
            counter++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}