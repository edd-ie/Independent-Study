# Instructions set

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
