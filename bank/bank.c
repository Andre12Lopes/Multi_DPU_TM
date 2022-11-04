#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <alloc.h>
#include <assert.h>
#include <barrier.h>
#include <perfcounter.h>

#include <norec.h>
#include <thread_def.h>
#include <defs.h>
#include <mram.h>

#include "macros.h"

#define TRANSFER    2
#define ACCOUNT_V   1000
#define BACH_SIZE   (N_TANSACTIONS * 2)

__host uint64_t n_tasklets;

BARRIER_INIT(my_barrier, NR_TASKLETS);

#ifdef ACC_IN_MRAM
int __mram_noinit bank[N_ACCOUNTS];
#else
int bank[N_ACCOUNTS];
#endif

void initialize_accounts();
void check_total();

#ifdef TX_IN_MRAM
Thread __mram_noinit t_mram[NR_TASKLETS];
#endif

__mram_noinit int bach[BACH_SIZE * NR_TASKLETS];

int main()
{
    #ifndef TX_IN_MRAM
    Thread t;
    #endif
    int ra, rb, tid;
    int a, b;
    uint64_t s;

    s = (uint64_t)me();
    tid = me();

#ifdef TX_IN_MRAM
    TxInit(&t_mram[tid], tid);
#else
    TxInit(&t, tid);
#endif

    initialize_accounts();

    if (me() == 0)
    {
        n_tasklets = NR_TASKLETS;
    }

    barrier_wait(&my_barrier);

    for (int i = 0; i < BACH_SIZE; i += 2)
    {
        ra = bach[(BACH_SIZE * tid) + i];
        rb = bach[(BACH_SIZE * tid) + (i + 1)];

#ifdef TX_IN_MRAM
            START(&t_mram[tid]);

            a = LOAD(&t_mram[tid], &bank[ra]);
            a -= TRANSFER;
            STORE(&t_mram[tid], &bank[ra], a);

            b = LOAD(&t_mram[tid], &bank[rb]);
            b += TRANSFER;
            STORE(&t_mram[tid], &bank[rb], b);

            COMMIT(&t_mram[tid]);
#else
            START(&t);

            a = LOAD(&t, &bank[ra]);
            a -= TRANSFER;
            STORE(&t, &bank[ra], a);

            b = LOAD(&t, &bank[rb]);
            b += TRANSFER;
            STORE(&t, &bank[rb], b);

            COMMIT(&t);
#endif
    }

    // ------------------------------------------------------

    barrier_wait(&my_barrier);

    check_total();
    
    return 0;
}

void initialize_accounts()
{
    if (me() == 0)
    {
        for (int i = 0; i < N_ACCOUNTS; ++i)
        {
            bank[i] = ACCOUNT_V;
        }    
    }
}

void check_total()
{
    if (me() == 0)
    {
        // printf("[");
        unsigned int total = 0;
        for (int i = 0; i < N_ACCOUNTS; ++i)
        {
            // printf("%u,", bank[i]);
            total += bank[i];
        }
        // printf("]\n");

        // printf("TOTAL = %u\n", total);

        assert(total == (N_ACCOUNTS * ACCOUNT_V));
    }
}