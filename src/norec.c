#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <alloc.h>
#include <perfcounter.h>

#include "norec.h"
#include "utils.h"

#define FILTERHASH(a)                   ((UNS(a) >> 2) ^ (UNS(a) >> 5))
#define FILTERBITS(a)                   (1 << (FILTERHASH(a) & 0x1F))

enum 
{
  TX_ACTIVE = 1,
  TX_COMMITTED = 2,
  TX_ABORTED = 4
};

#include "thread_def.h"

volatile long *LOCK;

// --------------------------------------------------------------

static inline unsigned long long 
MarsagliaXORV(unsigned long long x)
{
    if (x == 0)
    {
        x = 1;
    }

    x ^= x << 6;
    x ^= x >> 21;
    x ^= x << 7;

    return x;
}

static inline unsigned long long 
MarsagliaXOR(TYPE unsigned long long *seed)
{
    unsigned long long x = MarsagliaXORV(*seed);
    *seed = x;

    return x;
}

static inline unsigned long long 
TSRandom(TYPE Thread *Self)
{
    return MarsagliaXOR(&Self->rng);
}

static inline void 
backoff(TYPE Thread *Self, long attempt)
{
    unsigned long long stall = TSRandom(Self) & 0xF;
    stall += attempt >> 2;
    stall *= 10;
    
    // stall = stall << attempt;
    /* CCM: timer function may misbehave */
    volatile unsigned long long i = 0;
    while (i++ < stall)
    {
        PAUSE();
    }
}

void 
TxAbort(TYPE Thread *Self)
{
    Self->Aborts++;

#ifdef BACKOFF
    Self->Retries++;
    if (Self->Retries > 3)
    { /* TUNABLE */
        backoff(Self, Self->Retries);
    }
#endif

    Self->status = TX_ABORTED;

    // SIGLONGJMP(*Self->envPtr, 1);
    // ASSERT(0);
}

// --------------------------------------------------------------

static inline TYPE AVPair*
MakeList (long sz, TYPE Log* log)
{
    TYPE AVPair* ap = log->List;
    // assert(ap);

    memset(ap, 0, sizeof(*ap) * sz);

    TYPE AVPair* List = ap;
    TYPE AVPair* Tail = NULL;
    long i;
    for (i = 0; i < sz; i++) {
        TYPE AVPair* e = ap++;
        e->Next    = ap;
        e->Prev    = Tail;
        e->Ordinal = i;
        Tail = e;
    }
    Tail->Next = NULL;

    return List;
}

void 
TxInit(TYPE Thread *t, long id)
{
    /* CCM: so we can access NOREC's thread metadata in signal handlers */
    // pthread_setspecific(global_key_self, (void*)t);

    memset(t, 0, sizeof(*t)); /* Default value for most members */

    t->UniqID = id;
    t->rng = id + 1;
    t->xorrng[0] = t->rng;
    t->Starts = 0;
    t->Aborts = 0;
    
    t->process_cycles = 0;
    t->commit_cycles = 0;

    MakeList(NOREC_INIT_NUM_ENTRY, &(t->wrSet));
    t->wrSet.put = t->wrSet.List;

    MakeList(NOREC_INIT_NUM_ENTRY, &(t->rdSet));
    t->rdSet.put = t->rdSet.List;
}

// --------------------------------------------------------------

static inline void
txReset (TYPE Thread* Self)
{
    Self->wrSet.put = Self->wrSet.List;
    Self->wrSet.tail = NULL;

    Self->wrSet.BloomFilter = 0;
    Self->rdSet.put = Self->rdSet.List;
    Self->rdSet.tail = NULL;

    Self->status = TX_ACTIVE;
}

void 
TxStart(TYPE Thread *Self)
{
    txReset(Self);

    Self->read_cycles = 0;
    Self->write_cycles = 0;
    Self->validation_cycles = 0;

    MEMBARLDLD();

    // Self->envPtr = envPtr;
    Self->time = perfcounter_config(COUNT_CYCLES, false);
    if (Self->start_time == 0)
    {
        Self->start_time = perfcounter_config(COUNT_CYCLES, false);
    }
    
    Self->Starts++;

    do
    {
        Self->snapshot = *LOCK;
    } while ((Self->snapshot & 1) != 0);
}

// --------------------------------------------------------------

// returns -1 if not coherent
static inline long 
ReadSetCoherent(TYPE Thread *Self)
{
    long time;
    while (1)
    {
        MEMBARSTLD();
        time = *LOCK;
        if ((time & 1) != 0)
        {
            continue;
        }

        TYPE Log *const rd = &Self->rdSet;
        TYPE AVPair *const EndOfList = rd->put;
        TYPE AVPair *e;
        for (e = rd->List; e != EndOfList; e = e->Next)
        {
            if (e->Valu != LDNF(e->Addr))
            {
                return -1;
            }
        }

        if (*LOCK == time)
        {
            break;
        }
    }
    return time;
}

// inline AVPair *
// ExtendList(AVPair *tail)
// {
//     AVPair *e = (AVPair *)malloc(sizeof(*e));
//     assert(e);
//     memset(e, 0, sizeof(*e));
//     tail->Next = e;
//     e->Prev = tail;
//     e->Next = NULL;
//     e->Ordinal = tail->Ordinal + 1;
//     return e;
// }

intptr_t 
TxLoad(TYPE Thread *Self, volatile __mram_ptr intptr_t *Addr)
{
    intptr_t Valu;

    Self->start_read = perfcounter_config(COUNT_CYCLES, false);

    // if (Self->snapshot == -1)
    // {
    //     printf("TID = %ld\n", Self->UniqID);
    // }

    intptr_t msk = FILTERBITS(Addr);
    if ((Self->wrSet.BloomFilter & msk) == msk)
    {
        TYPE Log *wr = &Self->wrSet;
        TYPE AVPair *e;
        for (e = wr->tail; e != NULL; e = e->Prev)
        {
            if (e->Addr == Addr)
            {
                return e->Valu;
            }
        }
    }

    // if (Self->snapshot == -1)
    // {
    //     printf("2 TID = %ld\n", Self->UniqID);
    // }

    MEMBARLDLD();
    Valu = LDNF(Addr);
    while (*LOCK != Self->snapshot)
    {
        Self->start_validation = perfcounter_config(COUNT_CYCLES, false);

        long newSnap = ReadSetCoherent(Self);

        Self->validation_cycles += perfcounter_get() - Self->start_validation;

        if (newSnap == -1)
        {
            TxAbort(Self);
            return 0;
        }

        Self->snapshot = newSnap;
        MEMBARLDLD();
        Valu = LDNF(Addr);
    }

    TYPE Log *k = &Self->rdSet;
    TYPE AVPair *e = k->put;
    if (e == NULL)
    {
    	printf("[WARNING] Reached RS extend\n");
    	assert(0);

        // k->ovf++;
        // e = ExtendList(k->tail);
        // k->end = e;
    }

    k->tail = e;
    k->put = e->Next;
    e->Addr = Addr;
    e->Valu = Valu;

    Self->read_cycles += perfcounter_get() - Self->start_read;

    return Valu;
}

// --------------------------------------------------------------

void 
TxStore(TYPE Thread *Self, volatile __mram_ptr intptr_t *addr, intptr_t valu)
{
    Self->start_write = perfcounter_config(COUNT_CYCLES, false);

    TYPE Log *k = &Self->wrSet;

    k->BloomFilter |= FILTERBITS(addr);

    TYPE AVPair *e = k->put;
    if (e == NULL)
    {
    	printf("[WARNING] Reached WS extend\n");
    	assert(0);

        // k->ovf++;
        // e = ExtendList(k->tail);
        // k->end = e;
    }

    k->tail = e;
    k->put = e->Next;
    e->Addr = addr;
    e->Valu = valu;

    Self->write_cycles += perfcounter_get() - Self->start_write;
}

// --------------------------------------------------------------

static inline void 
txCommitReset(TYPE Thread *Self)
{
    txReset(Self);
    Self->Retries = 0;

    Self->status = TX_COMMITTED;
}

static inline void 
WriteBackForward(TYPE Log *k)
{
    TYPE AVPair *e;
    TYPE AVPair *End = k->put;
    for (e = k->List; e != End; e = e->Next)
    {
        *(e->Addr) = e->Valu;
    }
}

static inline long 
TryFastUpdate(TYPE Thread *Self)
{
    TYPE Log *const wr = &Self->wrSet;
    perfcounter_t s_time;
    // long ctr;

acquire:
    acquire(LOCK);

    if (*LOCK != Self->snapshot)
    {
    	release(LOCK);

        s_time = perfcounter_config(COUNT_CYCLES, false);

    	long newSnap = ReadSetCoherent(Self);
        if (newSnap == -1)
        {
            return 0; // TxAbort(Self);
        }

        Self->total_commit_validation_cycles += perfcounter_get() - s_time;

        Self->snapshot = newSnap;

        goto acquire;
    }

    *LOCK = Self->snapshot + 1;

    release(LOCK);

    {
        WriteBackForward(wr); /* write-back the deferred stores */
    }

    MEMBARSTST(); /* Ensure the above stores are visible  */
    *LOCK = Self->snapshot + 2;
    MEMBARSTLD();

    return 1; /* success */
}

int 
TxCommit(TYPE Thread *Self)
{
    uint64_t t_process_cycles;

    t_process_cycles = perfcounter_get() - Self->time;
    Self->time = perfcounter_config(COUNT_CYCLES, false);

    /* Fast-path: Optional optimization for pure-readers */
    if (Self->wrSet.put == Self->wrSet.List)
    {
        txCommitReset(Self);
        Self->commit_cycles += perfcounter_get() - Self->time;
        Self->total_cycles += perfcounter_get() - Self->start_time;
        Self->process_cycles += t_process_cycles;
        Self->total_read_cycles += Self->read_cycles;
        Self->total_write_cycles += Self->write_cycles;
        Self->total_validation_cycles += Self->validation_cycles;

        Self->start_time = 0;
        return 1;
    }

    if (TryFastUpdate(Self))
    {
        txCommitReset(Self);
        Self->commit_cycles += perfcounter_get() - Self->time;
        Self->total_cycles += perfcounter_get() - Self->start_time;
        Self->process_cycles += t_process_cycles;
        Self->total_read_cycles += Self->read_cycles;
        Self->total_write_cycles += Self->write_cycles;
        Self->total_validation_cycles += Self->validation_cycles;

        Self->start_time = 0;
        return 1;
    }

    TxAbort(Self);
    
    return 0;
}