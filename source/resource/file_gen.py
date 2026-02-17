import os

def generate_test_files(num_files=10, chunk_size=4096):
    # Create a directory for test data if it doesn't exist
    folder = "resource/test_data"
    if not os.path.exists(folder):
        os.makedirs(folder)

    print(f"Generating {num_files} files in '{folder}/'...")

    for file_idx in range(num_files):
        filename = os.path.join(folder, f"test_file_{file_idx}.txt")
        
        with open(filename, "w") as f:
            # We'll make each file 2 chunks large
            for block_idx in range(2):
                header = f"--- File {file_idx} | Block {block_idx} ---\n"
                f.write(header)
                
                # Fill remaining space in the 4096 chunk
                # Using a unique character for each file to verify output
                char = chr(65 + (file_idx % 26)) # A, B, C...
                padding = (char * 5 + " ") * ((chunk_size // 6) - len(header))
                f.write(padding + "\n")
                
    print("Done. To run your test, use:")
    print(f"./your_program test_data/test_file_*.txt")

if __name__ == "__main__":
    # You can increase num_files to 64 to match your QUEUE_DEPTH
    generate_test_files(num_files=64, chunk_size=4096)