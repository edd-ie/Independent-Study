import matplotlib.pyplot as plt
import re
import sys

def generate_chart(input_file='results.txt', output_file='benchmark_results.png'):
    try:
        with open(input_file, 'r') as f:
            content = f.read()
    except FileNotFoundError:
        print(f"Error: {input_file} not found.")
        return

    # Extract Mode Names and Throughput values using Regex
    # Matches: "Result for Tree Broadcast (Mode 1):" and "Throughput: 1250.45 MB/s"
    modes = re.findall(r"Result for (.*?):", content)
    throughputs = [float(t) for t in re.findall(r"Throughput:\s+([\d.]+)\s+MB/s", content)]

    if not modes or not throughputs:
        print("No valid benchmark data found in the file.")
        return

    # Combine and sort by throughput (fastest to slowest)
    data = sorted(zip(modes, throughputs), key=lambda x: x[1], reverse=True)
    sorted_modes, sorted_throughputs = zip(*data)

    # Plotting
    plt.figure(figsize=(10, 6))
    bars = plt.bar(sorted_modes, sorted_throughputs, color=['#2ecc71', '#3498db', '#9b59b6', '#e67e22'])
    
    # Add value labels on top of bars
    for bar in bars:
        yval = bar.get_height()
        plt.text(bar.get_x() + bar.get_width()/2, yval + 10, f'{yval:.2f}', ha='center', va='bottom', fontweight='bold')

    plt.ylabel('Throughput (MB/s)', fontsize=12, fontweight='bold')
    plt.title('Broadcaster Performance Comparison', fontsize=14, fontweight='bold')
    plt.xticks(rotation=15, ha='right')
    plt.grid(axis='y', linestyle='--', alpha=0.6)
    
    plt.tight_layout()
    plt.savefig(output_file, dpi=300)
    print(f"Success: Chart saved as {output_file}")

if __name__ == "__main__":
    file_to_parse = sys.argv[1] if len(sys.argv) > 1 else 'results.txt'
    generate_chart(file_to_parse)