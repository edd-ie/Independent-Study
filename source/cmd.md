# Instructions set

## Running instructions:

### Scenario 1: File → Broadcaster → (Screen + File)

This demonstrates taking a static file, splitting it, and sending it to two different types of destinations.

1. **Terminal 1 (The Screen Listener):** Wait for data and print it to this window

```bash
cat < out1.fifo

# Persistent
while true; do cat < out1.fifo; done
```

2. **Terminal 2 (The File Listener):** Wait for data and save it to a physical file named `captured.txt`.

```bash
cat < out2.fifo > captured.txt
```

4. **Terminal 4 (The Source):** Pour the file into the input pipe.

```bash
cat resource/test_input.txt > input.fifo
```

3. **Terminal 3 (The Broadcaster):**

```bash
#./main input.fifo out1.fifo out2.fifo
make broadcast
```

**Result:** You’ll see the text scroll in Terminal 1, and Terminal 2 will finish. You can verify Terminal 2 worked by running `cat captured.txt`.

---

### Scenario 2: Interactive Typing → Multiple Terminals

This turns your C++ program into a "Chat Server" of sorts.

1. **Terminal 1 (Listener A):**

```bash
cat < out1.fifo
```

2. **Terminal 2 (Listener B):**

```bash
cat < out2.fifo
```

3. **Terminal 3 (The Broadcaster):**

```bash
./main input.fifo out1.fifo out2.fifo
```

4. **Terminal 4 (The Source):** Open an interactive writing session.

```bash
cat > input.fifo
```

_Now, type a sentence and hit **Enter**. It will appear instantly in Terminals 1 and 2._

Running:

```
mpirun -np 4 ./build/release/main
```

Test target for MPI:

```
make test n=4
```

parallel compilation:

```
make -j
```

Switch modes:

```
make MODE=debug
make MODE=release
```

clang-format integration:

```
make format
```

Debug/Release build directories:

- `build/debug/`
- `build/release/`
