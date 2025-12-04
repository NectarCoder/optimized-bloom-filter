# Optimized Bloom Filter

This repo contains a two Bloom filter implementations in Python as well as C - a standardized bloom filter and an optimized lightweight bloom filter.  
We compare both implementations via test suite that tests membership & empirical false-positive rates, as well as real-time performance like throughput and time taken for operations.  
For testing purposes we use a synthetic dataset of UUIDs.   

## Key points THIS SECTION WIP
- WIP

## Python implementation (cross-platform)
1. Create and activate a virtual environment and install dependencies (scripts are in `setup_venv_scripts/`):

```bash  
# From project root
.\setup_venv_scripts\setup_venv.ps1 # For Windows
./setup_venv_scripts/setup_venv.sh # For Linux
./setup_venv_scripts/setup_venv.zsh # For macOS
```

2. Run the python test suite:

```bash
cd python_impl
python -m test_suite
```

## C implementation (UNIX)
If you want to build and run the C benchmark on a UNIX-like system, you need a POSIX shell, `gcc`, and `make` installed.  

> [!IMPORTANT]  
> Ensure you run `git submodule init` and `git submodule update` before building to pull the C implementations for murmurhash and xxHash. 

Build and run using the Makefile:
```bash
cd c_impl
make
./bloom_bench.out
```

Or compile in a single command (optimized):
```bash
gcc -O3 test_bloom_filters.c bloom_filter.c lightweight_bloom_filter.c -o bloom_bench.out
./bloom_bench.out
```

Convenience targets:
- `make run`  — build and run `bloom_bench.out`
- `make clean` — remove the built binary and object files

