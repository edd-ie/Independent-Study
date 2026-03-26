#!/bin/bash
# benchmark.sh
OUTPUT_CSV="./resource/benchmark_results.csv"
INPUT_FILE="./resource/test_data/large_simulation_file.bin"
FILE_SIZE_GB=1
COPIES=4

# Header for CSV
echo "Mode,Throughput_MBs,Time_s" > $OUTPUT_CSV

# Modes: 1=Readv, 2=Uring_Vec, 3=Splice, 4=Tee (Your current implementations)
for mode in 2 ; do
    echo "Testing Mode $mode..."
    for i in {1..5}; do
        # CLEAR CACHE: Critical for "Upper Limit" testing
        sync; echo 3 | sudo tee /proc/sys/vm/drop_caches > /dev/null
        
        # Run and capture only the numeric stats
        # Assuming your print_stats outputs "Result for ... Time: X s Total Data: Y MB Throughput: Z MB/s"
        if [$mode -eq 1] then
            RESULT=$(./build/release/main $mode $INPUT_FILE $COPIES)
        else 
            RESULT=$(./build/release/main $mode $INPUT_FILE $COPIES)
        fi
        
        TIME=$(echo "$RESULT" | grep "Time:" | awk '{print $2}')
        TPS=$(echo "$RESULT" | grep "Throughput:" | awk '{print $2}')
        
        echo "$mode,$TPS,$TIME" >> $OUTPUT_CSV
    done
done

# Run CMD
# sudo stdbuf -oL ./benchmark.sh

