#include <iostream>

#include "lab12_scheduler.h"
#include "lab12_semaphore.h"

void waste_time(const int x) {
    std::cout << "Waste time....................Simulating CPU work!......................"
              << std::endl;

    volatile unsigned long long accumulator = 0;
    for (unsigned int i = 0; i < 100000U * static_cast<unsigned int>(x); ++i) {
        for (unsigned short j = 1; j > 0; --j) {
            accumulator += j + i;
        }
    }

    (void)accumulator;
}

int main() {
    scheduler swapper;
    int t_id = -1;

    std::cout << "\nStep 1 & 2: Create a scheduler and attempt to create 4 tasks." << std::endl;
    for (int i = 0; i < 4; ++i) {
        t_id = swapper.create_task();
    }
    (void)t_id;

    swapper.dump();

    std::cout << "Step 3: Start the scheduler." << std::endl;
    swapper.start();
    swapper.dump();

    std::cout << "Step 4: Try some context switching (3 yields)." << std::endl;
    for (int i = 0; i < 3; ++i) {
        waste_time(3);
        swapper.yield();
        swapper.dump();
    }

    std::cout << "Step 5: Test the scheduler and semaphore together." << std::endl;
    semaphore resource1_sema(1, "resource1", &swapper);
    resource1_sema.dump(0);

    t_id = swapper.get_task_id();
    std::cout << "Task " << t_id
              << " is trying to obtain the semaphore (Resource1)" << std::endl;
    resource1_sema.down(t_id);
    swapper.dump();
    waste_time(3);
    swapper.yield();
    swapper.dump();

    t_id = swapper.get_task_id();
    std::cout << "Task " << t_id
              << " is trying to obtain the semaphore (Resource1)" << std::endl;
    resource1_sema.down(t_id);
    swapper.dump();
    waste_time(3);
    swapper.yield();
    swapper.dump();

    t_id = swapper.get_task_id();
    std::cout << "Task " << t_id
              << " is trying to obtain the semaphore (Resource1)" << std::endl;
    resource1_sema.down(t_id);
    swapper.dump();
    waste_time(3);
    swapper.yield();
    swapper.dump();

    t_id = swapper.get_task_id();
    std::cout << "Task " << t_id
              << " is trying to release the semaphore (Resource1)" << std::endl;
    swapper.dump();
    resource1_sema.up();
    swapper.dump();
    waste_time(3);
    swapper.yield();
    swapper.dump();

    return 0;
}
