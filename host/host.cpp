#include <chrono>
#include <dpu>
#include <iostream>
#include <ostream>
#include <random>
#include <unistd.h>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>

using namespace dpu;

#define BACH_SIZE   (N_TANSACTIONS * 2) // Batch for each DPU taskel
#define NR_TASKLETS 11

std::mutex m;
std::mutex start_m;
std::condition_variable cond;

int n_threads_waiting;

std::atomic<uint64_t> total_exec_time;
std::atomic<uint64_t> total_copy_time;

void create_bach(DpuSet &system, std::vector<std::vector<int>> &bach);
void do_work(int tid, int n_dpus);
void start_barrier();

int main(int argc, char **argv)
{
    std::vector<std::thread> threads;
    double total_time = 0;

    n_threads_waiting  = 0;

    total_copy_time = 0;
    total_exec_time = 0;

    for (int i = 0; i < N_THREADS; ++i)
    {
        threads.push_back(std::thread(do_work, i, N_DPUS / N_THREADS));
    }

    start_barrier();

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < N_THREADS; ++i)
    {
        threads[i].join();
    }

    auto end = std::chrono::steady_clock::now();
    total_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    // std::vector<std::vector<uint32_t>> n_tasklets(1);
    // double total_copy_time = 0;
    // double total_time = 0;

    // try
    // {
    //     auto system = DpuSet::allocate(N_DPUS);

    //     system.load("bank/bank");

    //     std::vector<std::vector<int>> bach(system.dpus().size(), std::vector<int>(BACH_SIZE * NR_TASKLETS));
    //     for (int i = 0; i < N_BACHES; ++i)
    //     {
    //         create_bach(system, bach);

    //         auto start = std::chrono::steady_clock::now();
        
    //         system.copy("bach", bach);

    //         auto end_copy = std::chrono::steady_clock::now();

    //         system.exec();

    //         auto end = std::chrono::steady_clock::now();

    //         total_copy_time += std::chrono::duration_cast<std::chrono::microseconds>(end_copy - start).count();
    //         total_time += std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    //     }

    //     auto dpu = system.dpus()[0];
    //     n_tasklets.front().resize(1);
    //     dpu->copy(n_tasklets, "n_tasklets");

    std::cout << "11" << "\t"
              << N_TANSACTIONS << "\t"
              << N_BACHES << "\t"
              << N_DPUS << "\t"
              << ((double) total_copy_time) / N_THREADS << "\t"
              << ((double) total_exec_time) / N_THREADS << "\t"
              << total_time << std::endl;
    // }
    // catch (const DpuError &e)
    // {
    //     std::cerr << e.what() << std::endl;
    // }

    return 0;
}

void do_work(int tid, int n_dpus)
{
    std::unique_lock<std::mutex> unique_lock(m);
    dpu::DpuSet system = dpu::DpuSet::allocate(n_dpus);
    unique_lock.unlock();

    std::vector<std::vector<int>> bach(system.dpus().size(), std::vector<int>(BACH_SIZE * NR_TASKLETS));
    try
    {
        system.load("bank/bank");

        start_barrier();
        
        for (int i = 0; i < N_BACHES; ++i)
        {
            create_bach(system, bach);

            auto start = std::chrono::steady_clock::now();
        
            system.copy("bach", bach);

            auto end_copy = std::chrono::steady_clock::now();

            system.exec();

            auto end = std::chrono::steady_clock::now();

            total_copy_time += std::chrono::duration_cast<std::chrono::microseconds>(end_copy - start).count();
            total_exec_time += std::chrono::duration_cast<std::chrono::microseconds>(end - end_copy).count();
        }
    }
    catch (const DpuError &e)
    {
        std::cerr << e.what() << std::endl;
    }
}

void start_barrier()
{
    std::unique_lock<std::mutex> unique_lock(start_m);

    if (++n_threads_waiting == (N_THREADS + 1))
    {
        unique_lock.unlock();
        cond.notify_all();

        return;
    }
    else
    {
        cond.wait(unique_lock);
    }

    unique_lock.unlock();
}

void create_bach(DpuSet &system, std::vector<std::vector<int>> &bach)
{
    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<std::mt19937::result_type> rand(0, N_ACCOUNTS - 100);

    for (unsigned i = 0; i < system.dpus().size(); ++i)
    {
        for (int j = 0; j < (BACH_SIZE * NR_TASKLETS); ++j)
        {
            bach[i][j] = rand(rng);
        }
    }
}