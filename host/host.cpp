#include <chrono>
#include <dpu>
#include <iostream>
#include <ostream>
#include <random>
#include <unistd.h>

using namespace dpu;

#define BACH_SIZE (N_TANSACTIONS * 2)

void create_bach(DpuSet &system);

int main(int argc, char **argv)
{
    std::vector<std::vector<uint32_t>> nThreads(1);

    try
    {
        auto system = DpuSet::allocate(N_DPUS);

        system.load("bank/bank");

        create_bach(system);

        auto start = std::chrono::steady_clock::now();

        system.exec();

        auto end = std::chrono::steady_clock::now();

        auto dpu = system.dpus()[0];
        nThreads.front().resize(1);
        dpu->copy(nThreads, "n_tasklets");

        std::cout << (double) nThreads.front().front() << "\t"
                  << N_DPUS << "\t"
                  << N_TANSACTIONS << "\t"
                  << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << std::endl;
    }
    catch (const DpuError &e)
    {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}

void create_bach(DpuSet &system)
{
    std::vector<std::vector<int>> bach(system.dpus().size(), std::vector<int>(BACH_SIZE));

    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<std::mt19937::result_type> rand(0, N_ACCOUNTS);

    for (unsigned i = 0; i < system.dpus().size(); ++i)
    {
        for (int j = 0; j < BACH_SIZE; ++j)
        {
            bach[i][j] = rand(rng);
        }
    }

    system.copy("bach", bach);
}