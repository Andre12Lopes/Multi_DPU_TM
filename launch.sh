#!/bin/bash
echo -e "N_THREADS_DPU\tN_TANSACTIONS\tN_BACHES\tN_DPUS\tCOPY_TIME\tEXEC_TIME\tTOTAL_TIME" > results.txt

make clean
# make test NR_BACHES=10 NR_TANSACTIONS=1000 NR_ACCOUNTS=800 NR_DPUS=1

# for (( j = 0; j < 10; j++ )); do
# 	./host/host >> results.txt
# done

for (( i = 128; i < 2560; i += 128 )); do
	make clean
	make test NR_BACHES=10 NR_TANSACTIONS=1000 NR_ACCOUNTS=800 NR_DPUS=$i NR_THREADS=$((i/64))
	
	for (( j = 0; j < 10; j++ )); do
		./host/host >> results.txt
	done
done

# make clean
# make test NR_BACHES=10 NR_TANSACTIONS=1000 NR_ACCOUNTS=800 NR_DPUS=2554

# for (( j = 0; j < 10; j++ )); do
# 	./host/host >> results.txt
# done