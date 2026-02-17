# gen_test.py
chunk_size = 4096  # Match the CHUNK_SIZE in your C++ code
num_chunks = 5     # Create 5 chunks worth of data

with open("test_input.txt", "w") as f:
    for i in range(num_chunks + 1):
        line = f"--- Start of Block {i} ---\n"
        f.write(line)
        # Fill the rest of the chunk with repetitive data
        padding = "Data " * ((chunk_size // 5) - len(line))
        f.write(padding + "\n")

print("Generated test_input.txt")