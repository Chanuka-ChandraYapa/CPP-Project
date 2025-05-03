#include "../include/ThreadManager.h"
#include <stdexcept>
#include <iostream>

ThreadManager::ThreadManager(size_t numThreads) 
    : numThreads(numThreads), running(false), activeThreads(0), pendingTasks(0) {
    if (numThreads <= 0) {
        throw std::invalid_argument("Number of threads must be positive");
    }
    
    // Initialize thread load tracking using a unique_ptr to array of atomics
    threadLoads = std::make_unique<std::atomic<size_t>[]>(numThreads);
    for (size_t i = 0; i < numThreads; ++i) {
        threadLoads[i] = 0;
    }
    
    start();  // Automatically start the thread pool
}

ThreadManager::~ThreadManager() {
    stop();
}

void ThreadManager::start() {
    if (running) {
        throw std::runtime_error("Thread manager is already running");
    }
    running = true;
    threads.clear();
    
    for (size_t i = 0; i < numThreads; ++i) {
        threads.emplace_back([this, i]() {
            while (running) {
                // Execute task if available
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(taskMutex);
                    taskCondition.wait(lock, [this]() { 
                        return !running || !taskQueue.empty(); 
                    });
                    
                    if (!running && taskQueue.empty()) {
                        break;
                    }
                    
                    if (!taskQueue.empty()) {
                        task = std::move(taskQueue.front());
                        taskQueue.pop();
                    }
                }
                
                if (task) {
                    ++activeThreads;
                    --pendingTasks;  // Task is now being processed
                    
                    try {
                        // Update thread load counter
                        threadLoads[i]++;
                        
                        // Execute the task
                        task();
                    } 
                    catch (const std::exception& e) {
                        // Handle exceptions from tasks
                        // In a real application, you might want to store the exception
                        // or provide a way for the user to define an exception handler
                        std::cerr << "Task exception: " << e.what() << std::endl;
                    }
                    
                    --activeThreads;
                    
                    // Use lock when checking completion condition to avoid race conditions
                    std::lock_guard<std::mutex> completionLock(completionMutex);
                    if (pendingTasks == 0 && activeThreads == 0) {
                        completionCondition.notify_all();
                    }
                }
            }
        });
    }
}

void ThreadManager::stop() {
    {
        std::lock_guard<std::mutex> lock(taskMutex);
        running = false;
    }
    taskCondition.notify_all();
    
    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

void ThreadManager::addTask(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(taskMutex);
        taskQueue.push(std::move(task));
        ++pendingTasks;
    }
    taskCondition.notify_one();
}

bool ThreadManager::isRunning() const {
    return running;
}

size_t ThreadManager::getNumThreads() const {
    return numThreads;
}

void ThreadManager::setNumThreads(size_t newNumThreads) {
    if (running) {
        throw std::runtime_error("Cannot change thread count while running");
    }
    numThreads = newNumThreads;
}

size_t ThreadManager::getTaskCount() const {
    std::lock_guard<std::mutex> lock(taskMutex);
    return taskQueue.size();
}

void ThreadManager::waitForCompletion() {
    if (!running) return;  // Don't wait if thread manager isn't running
    
    std::unique_lock<std::mutex> lock(completionMutex);
    completionCondition.wait(lock, [this]() {
        return pendingTasks == 0 && activeThreads == 0;
    });
}

double ThreadManager::getAverageThreadLoad() const {
    if (numThreads == 0) {
        return 0.0;
    }
    
    size_t totalLoad = 0;
    for (size_t i = 0; i < numThreads; ++i) {
        totalLoad += threadLoads[i].load();
    }
    return static_cast<double>(totalLoad) / numThreads;
}

size_t ThreadManager::getActiveThreadCount() const {
    return activeThreads;
}

// Function was removed from header, so removing implementation as well 