#include <chrono>
#include <dpu>
#include <iostream>
#include <ostream>
#include <random>
#include <unistd.h>

using namespace dpu;

#define BACH_SIZE (N_TANSACTIONS * 2)
#define NR_TASKLETS 11

void create_bach(DpuSet &system, std::vector<std::vector<int>> &bach);

int main(int argc, char **argv)
{
    std::vector<std::vector<uint32_t>> nThreads(1);
    double total_copy_time = 0;
    double total_time = 0;

    try
    {
        auto system = DpuSet::allocate(N_DPUS);

        system.load("bank/bank");

        std::vector<std::vector<int>> bach(system.dpus().size(), std::vector<int>(BACH_SIZE * NR_TASKLETS));
        for (int i = 0; i < N_BACHES; ++i)
        {
            create_bach(system, bach);

            auto start = std::chrono::steady_clock::now();
        
            system.copy("bach", bach);

            auto end_copy = std::chrono::steady_clock::now();

            system.exec();

            auto end = std::chrono::steady_clock::now();

            total_copy_time += std::chrono::duration_cast<std::chrono::microseconds>(end_copy - start).count();
            total_time += std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        }

        auto dpu = system.dpus()[0];
        nThreads.front().resize(1);
        dpu->copy(nThreads, "n_tasklets");

        std::cout << (double) nThreads.front().front() << "\t"
                  << N_TANSACTIONS << "\t"
                  << N_BACHES << "\t"
                  << N_DPUS << "\t"
                  << total_copy_time << "\t"
                  << total_time << std::endl;
    }
    catch (const DpuError &e)
    {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}

void create_bach(DpuSet &system, std::vector<std::vector<int>> &bach)
{
    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<std::mt19937::result_type> rand(0, N_ACCOUNTS);

    for (unsigned i = 0; i < system.dpus().size(); ++i)
    {
        for (int j = 0; j < (BACH_SIZE * NR_TASKLETS); ++j)
        {
            bach[i][j] = rand(rng);
        }
    }
}