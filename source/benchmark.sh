#!/bin/bash
# benchmark.sh
OUTPUT_CSV="./resource/benchmark_results.csv"
INPUT_FILE="./test/data.txt"
# INPUT_FILE="./resource/test_data/large_simulation_file.bin"
# INPUT_FILE="./test/test_data.txt"
COPIES=3



echo "Mode,Throughput_MBs,Time_s" > "$OUTPUT_CSV"

for mode in {1..4}; do
    echo "Testing Mode $mode..."
    for i in {1..4}; do
        # Clear Cache
        sync && echo 3 | sudo tee /proc/sys/vm/drop_caches > /dev/null
        
        # Execute
        if [ "$mode" -eq 1 ]; then
            RESULT=$(OMP_NUM_THREADS=4 ./build/release/main "$mode" "$INPUT_FILE" "$COPIES")
        else 
            RESULT=$(./build/release/main "$mode" "$INPUT_FILE" "$COPIES")
        fi
        
        # Parse stats - \s+ matches one or more whitespace characters
        TIME=$(echo "$RESULT" | grep -Po 'Time:\s+\K[0-9.]+')
        TPS=$(echo "$RESULT" | grep -Po 'Throughput:\s+\K[0-9.]+')
        
        # Append to CSV only if we actually got a result
        if [ -n "$TPS" ]; then
            echo "$mode,$TPS,$TIME" >> "$OUTPUT_CSV"
            echo "   Trial $i: $TPS MB/s in $TIME s"
        else
            echo "   Trial $i: FAILED (Check binary output)"
        fi
    done
done

echo "sudo chown -R $(whoami):$(whoami) ."

# Run CMD
# sudo stdbuf -oL ./benchmark.sh

