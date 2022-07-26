#ifndef _MACROS_H_
#define _MACROS_H_

#define START(t)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        TxStart(t);

#define LOAD(t, addr)                                                                                                  \
	    TxLoad(t, addr);                                                                                               \
	    if ((t)->status == 4)                                                                                          \
	    {                                                                                                              \
	        continue;                                                                                                  \
	    }

#define STORE(t, addr, val)                                                                                            \
	    TxStore(t, addr, val);                                                                                         \
	    if ((t)->status == 4)                                                                                          \
	    {                                                                                                              \
	        continue;                                                                                                  \
	    }

#define COMMIT(t)                                                                                                      \
	    TxCommit(t);                                                                                                   \
	    if ((t)->status != 4)                                                                                          \
	    {                                                                                                              \
	        break;                                                                                                     \
	    }                                                                                                              \
    }                                                                                                                  \
    while (1)

#endif /* _MACROS_H_ */