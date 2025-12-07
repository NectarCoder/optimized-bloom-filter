# A Lightweight Bloom Filter for Resource Constrained Devices

This repo contains two Bloom filter implementations in C as well as Python - a standardized bloom filter and an optimized lightweight bloom filter.  
We compare both implementations via test suite that tests membership & empirical false-positive rates, as well as real-time performance like throughput and time taken for operations. For testing purposes we use a synthetic dataset of UUIDs.   

Team Members:
- Amrut Ramasamy
- Ahmed Amarak    
- Prateek Paudel

## Key points

### Standard Bloom Filter Implementation Methodology

Standard Bloom filter implementation follows the classical bit-array design. It stores membership information as a compact array of bits and uses k virtual hash functions to set and test individual bits. Two non-cryptographic hash functions (MurmurHash3 and xxHash64) are used together with the double-hashing technique: two base hashes are computed and combined to derive k distinct bit indices. This avoids computing k independent full hashes while yielding good distribution properties.  

Key ideas:
- Compact bit array (bit-level addressing)
- Double hashing (two base hashes => k indices)
- Uses MurmurHash and xxHash for fast, production-quality hashing

### Lightweight Bloom Filter Implementation Methodology

The lightweight bloom filter implements a locality-aware, block-based variant. Rather than scattering k bits across the entire bit array, each element maps to a single 64-bit block (word) and sets k bit positions within that block. The filter implementation does this by computing a single 64-bit digest using xxHash64, derives a block index from the higher 32-bit sector of the digest, and uses a cheap, deterministic PRNG (splitmix64) seeded by the lower 32-bit sector of the digest to generate k bit positions inside the chosen 64-bit block. The bit-array is organized as an array of 64-bit words whose length is rounded up to the next power of two for fast masks/indexing.

Key ideas:
- Block-based layout (64-bit words)
- Each element maps to a single block to set/check k bits there
- Digest & PRNG-driven per-block bit selection (xxHash64 + splitmix64)
- Word count as a power-of-two allowing mask-based wrapping for quick indexing

### Testing Methodology

- Dataset generation: We generate a synthetic dataset of Universally Unique Identifiers (UUID) v4 strings (configurable size, e.g., 10k). The dataset is split into a training set (inserts) and a test set (queries), typically an 80/20 split (also configurable).
- Filter sizing: We compute the required bit-array size (standard) or word pool (lightweight) from the number of training items and the chosen bits-per-item, respectively.
- Functional correctness: A membership test validates that items inserted into a filter are reported as present (ensuring no false negatives).
- Empirical false positive testing: A query test queries the filter with items from the held-out test set (which weren’t inserted) and measures the proportion of queries that return true, to calculate the false-positive rate.
- Collision stress-testing: We generate small variants of test items (suffix/prefix change, changed last character) and measure collision-induced false positives to examine how the filter reacts to near-duplicate inputs.
- Performance benchmarking: We time inserts and queries (seconds and operations-per-second), records memory footprint (bytes and megabytes of the filter), and gathers `insert` and `query` throughput numbers to compare runtime efficiency across implementations.

## C implementation (UNIX)
If you want to build and run the C benchmark on a UNIX-like system, you need a POSIX shell, `gcc`, `make` and `libuuid` installed.  

> [!IMPORTANT]  
> Ensure you run `git submodule init` and `git submodule update` before building to pull the C implementations for murmurhash and xxHash. 

Build and run using the Makefile:
```bash
# From project root
cd c_impl
make
./bloom_bench
```

Makefile targets:
- `make run`   - build and run `bloom_bench`
- `make clean` - remove the built binary and object files

## Python implementation (cross-platform)
We also wrote a beta Python implementation of both bloom filters along with a test suite - but it does not perform as well as the C implementation.  
It is mainly left in for reference.  
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

