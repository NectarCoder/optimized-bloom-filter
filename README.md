# Optimized Bloom Filter

This repo contains a two Bloom filter implementations in Python - standard and optimized.  
We compare both implementations via test suite that tests membership & empirical false-positive rates, as well as real-time performance like throughput and time taken for operations.  
For testing purposes we use the Brown corpus dataset.  

## Key points THIS SECTION WIP
- Bloom filter implemented in `bf_std/bloom_filter.py` using MurmurHash3 (`mmh3`) and xxHash64 (`xxhash`).
- Hashing uses the Kirsch–Mitzenmacher double-hashing technique to derive k=7 hash values from two base hashes.
- Deterministic 80/20 train/held-out split of the Brown corpus unique tokens is used for reproducible evaluation.
- The consolidated test suite is `test_suite.py`. It sizes the filter to 10× the training-set size by default.

What the test suite does
- Loads normalized unique tokens from `dataset/brown.csv`.
- Sorts tokens deterministically, splits 80% train / 20% test.
- Builds a Bloom filter with size = 10 × len(train) and k = 7.
- Verifies all training items are present, measures empirical false-positive rate on the held-out test set, and reports simple collision statistics and memory usage.

## Getting Started
Quick start (Windows PowerShell)
1. Create and activate a virtual environment and install dependencies (scripts are in `setup_venv_scripts/`):

```bash
cd setup_venv_scripts
./setup_venv.ps1
# For Linux: ./setup_venv.sh
# For macOS: ./setup_venv.zsh
```

2. Run the consolidated test suite:

```bash
# From project root
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
gcc -O3 test_bloom_filters.c bloom_filter.c lightweight_bloom_filter.c hash_utils.c -o bloom_bench.out
./bloom_bench.out
```

Convenience targets:
- `make run`  — build and run `bloom_bench.out`
- `make clean` — remove the built binary and object files

