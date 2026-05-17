#pragma once
#include <gpiod.h> //use 🔥 Your Pi uses libgpiod v2 API
#include <iostream>
#include <thread>
#include <chrono>
#include <filesystem>
#include <atomic>
#include <mutex>
extern std::atomic<bool> runnung, running_recorder;
extern void gpio_task(void);
extern void writer_task(void);
extern void reader_task(void);
extern std::mutex mtx;
extern int counter;
