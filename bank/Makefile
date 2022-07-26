ROOT = ..

include ../Makefile.common

CC = dpu-upmem-dpurte-clang

NR_TASKLETS = 11

TARGET := bank

.PHONY:	all clean

all: $(TARGET)

%.o: %.c
	$(CC) -DNR_TASKLETS=$(NR_TASKLETS) $(CPPFLAGS) $(CFLAGS) $(DEFINES) -c -o $@ $<

bank.o:

$(TARGET): $(TARGET).o $(LIBNOREC)
	$(CC) -o $@ $< -DNR_TASKLETS=$(NR_TASKLETS) $(LDFLAGS)

clean:
	rm -f $(TARGET) *.o *.s