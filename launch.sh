!/bin/bash
echo -e "N_THREADS_DPU\tN_DPUS\tN_TXS\tTIME" > results.txt

for (( i = 1; i <= 10000; i *= 10 )); do
	make clean
	make test NR_TANSACTIONS=$i NR_ACCOUNTS=800 NR_DPUS=10
	
	for (( j = 0; j < 10; j++ )); do
		./host/host >> results.txt
	done
done
