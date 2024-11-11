#include "pool.h"

#include <atomic>
#include <iostream>
#include <sstream>
#include <vector>

struct EmptyTask : Task {
    void Run() override {
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
};

int main(int argc, char **argv) {
    ThreadPool pool{5};
    
    auto *et1 = new EmptyTask();
    pool.SubmitTask("first", et1);

    std::this_thread::sleep_for(std::chrono::milliseconds (500));

    auto *et2 = new EmptyTask();
    pool.SubmitTask("second", et2);

    pool.Stop();
    return 0;
}

