"""Consolidated Bloom filter test suite.

Performs a deterministic 80/20 split of unique tokens (sorted), builds the
filter with the 80% training set, and runs four tests:

1. Membership test on training set (should be all present)
2. False positive rate on held-out test set (real words not inserted)
3. Collision analysis using simple modifications of held-out words
4. Filter properties and memory usage

Filter size is set to 10x the number of training items, and we use 7 hash
functions (Kirsch-Mitzenmacher double hashing).
"""
from __future__ import annotations

import csv
import time
import uuid

from pathlib import Path
from typing import Tuple, List, Optional, Any, Type
import array

from bf_std.bloom_filter import BloomFilter
from bf_light.lightweight_bloom_filter import LightweightBloomFilter


NUM_HASHES = 7
DATASET_DIR = Path(__file__).parent / "dataset"


def load_unique_tokens() -> list[str]:
    """Load unique tokens from brown.csv and normalize them.
    
    Returns a sorted list of unique, normalized tokens.
    """
    words = set()
    csv_file = DATASET_DIR / "brown.csv"
    
    if not csv_file.exists():
        raise FileNotFoundError(f"Dataset file not found: {csv_file}")
    
    with open(csv_file, "r", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            tokens = row.get("tokenized_text", "").split()
            for token in tokens:
                # Normalize: lowercase and filter alphanumeric
                normalized = token.lower()
                if normalized and any(c.isalnum() for c in normalized):
                    words.add(normalized)
    
    return sorted(list(words))


def count_raw_tokens() -> int:
    """Count raw tokens in the CSV without any preprocessing.

    This counts all whitespace-separated tokens found in the
    `tokenized_text` column across all rows (including duplicates).
    """
    csv_file = DATASET_DIR / "brown.csv"

    if not csv_file.exists():
        raise FileNotFoundError(f"Dataset file not found: {csv_file}")

    total = 0
    with open(csv_file, "r", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            tokens = row.get("tokenized_text", "").split()
            total += len(tokens)

    return total


def build_split(words: Optional[list[str]] = None) -> Tuple[BloomFilter, list[str], list[str]]:
    """Create deterministic 80/20 split and build the bloom filter.

    If `words` is provided it will be used instead of loading from disk.

    Returns (bloom_filter, training_words, test_words).
    """
    if words is None:
        words = load_unique_tokens()

    n = len(words)
    split = int(n * 0.8)
    train = words[:split]
    test = words[split:]

    # Filter size = 10 * number of training items
    filter_size = max(1, len(train) * 10)
    bloom = BloomFilter(size=filter_size, num_hashes=NUM_HASHES)
    bloom.update(train)

    return bloom, train, test


def test_membership(bloom: Any, train: list[str]) -> None:
    """Verify all training items are present in the filter."""
    print("TEST A: Membership on training set")
    missing = [w for w in train if w not in bloom]
    print(f"  Training items: {len(train)}")
    print(f"  Missing after insertion: {len(missing)} (expected 0)")
    if missing:
        print(f"  Example missing: {missing[:5]}")
    print()


def test_false_positive_on_heldout(bloom: Any, train: list[str], test: list[str]) -> None:
    """Measure empirical false positive rate on held-out test set."""
    print("TEST B: False positive rate on held-out real words")
    test_filtered = [w for w in test if w not in train]
    
    if not test_filtered:
        print("  No held-out words available for testing.")
        return
    
    false_positives = sum(1 for w in test_filtered if w in bloom)
    fpr = false_positives / len(test_filtered)
    
    print(f"  Held-out words: {len(test_filtered)}")
    print(f"  False positives: {false_positives}")
    print(f"  Empirical FPR: {fpr:.6f} ({fpr*100:.4f}%)")
    print()


def test_collision_analysis(bloom: Any, train: list[str], test: list[str]) -> None:
    """Analyze collision rate using simple word modifications."""
    print("TEST C: Collision analysis with simple modifications of held-out words")
    sample = test[:500]
    modifications = []
    
    for word in sample:
        modifications.append(word + "x")
        if len(word) > 1:
            modifications.append(word[:-1] + "z")
        modifications.append("x" + word)

    # Remove any accidental actual words
    train_set = set(train)
    test_set = set(test)
    modifications = [m for m in modifications if m not in train_set and m not in test_set]
    
    if not modifications:
        print("  No modifications available for testing.")
        return
    
    false_positives = sum(1 for m in modifications if m in bloom)
    rate = false_positives / len(modifications)
    
    print(f"  Variants tested: {len(modifications)}")
    print(f"  False positives from variants: {false_positives}")
    print(f"  Collision rate: {rate:.6f} ({rate*100:.4f}%)")
    print()


def show_properties(bloom: Any, train: list[str]) -> None:
    """Display filter memory and configuration properties."""
    print("TEST D: Filter properties")
    # Compute byte-length of the underlying bit array for both standard
    # (bytearray) and the lightweight (array('Q')) implementations.
    bit_arr = bloom.bit_array
    if isinstance(bit_arr, (bytearray, bytes)):
        bytes_len = len(bit_arr)
    elif isinstance(bit_arr, array.array):
        # array('Q') stores 64-bit words => 8 bytes per entry
        bytes_len = len(bit_arr) * (array.array('Q').itemsize)
    else:
        # Fallback: attempt to compute length directly
        try:
            bytes_len = len(bit_arr)
        except Exception:
            bytes_len = 0
    mb = bytes_len / (1024 * 1024)
    
    print(f"  Filter size (bits): {bloom.size}")
    print(f"  Filter size (bytes): {bytes_len}")
    print(f"  Filter size (MB): {mb:.2f}")
    print(f"  Number of hash functions: {bloom.num_hashes}")
    print(f"  Words inserted: {len(train)}")
    print(f"  Bytes per word: {bytes_len / len(train):.4f}")
    print()

def test_performance(bloom: Any, train: list[str], test: list[str]) -> dict:
    """Measure insertion and query throughput (Ops/Sec)."""
    print("TEST E: Performance Benchmarking")
    
    print("  Benchmarking Insertions...")
    # Instantiate a fresh filter of same concrete type and config
    bloom_cls: Type[Any] = type(bloom)
    bench_filter = bloom_cls(bloom.size, bloom.num_hashes)
    
    start_time = time.perf_counter()
    for word in train:
        bench_filter.add(word)
    end_time = time.perf_counter()
    
    insert_time = end_time - start_time
    if insert_time <= 0:
        ops_per_sec = float('inf')
    else:
        ops_per_sec = len(train) / insert_time
    print(f"    - Inserted {len(train)} items in {insert_time:.4f} sec")
    print(f"    - Insertion Throughput: {ops_per_sec:,.0f} ops/sec")

    print("  Benchmarking Queries...")
    
    target_ops = 1_000_000
    repeats = (target_ops // len(test)) + 1
    large_test_set = test * repeats
    large_test_set = large_test_set[:target_ops]
    
    start_time = time.perf_counter()
    for word in large_test_set:
        _ = word in bench_filter
    end_time = time.perf_counter()
    
    query_time = end_time - start_time
    if query_time <= 0:
        query_ops_per_sec = float('inf')
    else:
        query_ops_per_sec = len(large_test_set) / query_time
    print(f"    - Performed {len(large_test_set)} queries in {query_time:.4f} sec")
    print(f"    - Query Throughput: {query_ops_per_sec:,.0f} ops/sec")
    print()

    metrics = {
        "insert_count": len(train),
        "insert_time": insert_time if insert_time > 0 else 0,
        "insert_ops_per_sec": ops_per_sec,
        "query_count": len(large_test_set),
        "query_time": query_time if query_time > 0 else 0,
        "query_ops_per_sec": query_ops_per_sec,
    }

    return metrics


def compare_performance(std_metrics: dict, light_metrics: dict) -> None:
    """Print a compact side-by-side comparison of performance metrics."""
    def fmt(val, nm=''):
        if val is None:
            return 'N/A'
        if isinstance(val, float):
            if val == float('inf'):
                return 'inf'
            if abs(val) >= 1000:
                return f"{val:,.0f}"
            return f"{val:,.2f}"
        return str(val)

    def pct_change(a, b):
        if a is None or a == 0:
            return None
        return (b - a) / a * 100

    print(f"{'Metric':<36}{'Standard':>18}{'Lightweight':>18}{'Diff (%)':>14}")
    print('-' * 86)

    rows = [
        ("Insertion Throughput (ops/sec)", std_metrics.get('insert_ops_per_sec'), light_metrics.get('insert_ops_per_sec')),
        ("Insertion Time (s)", std_metrics.get('insert_time'), light_metrics.get('insert_time')),
        ("Query Throughput (ops/sec)", std_metrics.get('query_ops_per_sec'), light_metrics.get('query_ops_per_sec')),
        ("Query Time (s)", std_metrics.get('query_time'), light_metrics.get('query_time')),
        ("Insert Count", std_metrics.get('insert_count'), light_metrics.get('insert_count')),
        ("Query Count", std_metrics.get('query_count'), light_metrics.get('query_count')),
    ]

    for name, std_val, light_val in rows:
        diff = pct_change(std_val, light_val)
        diff_str = f"{diff:+.2f}%" if diff is not None else 'N/A'
        print(f"{name:<36}{fmt(std_val):>18}{fmt(light_val):>18}{diff_str:>14}")
    print()


def generate_synthetic_data(n: int = 1_000_000) -> list[str]:
    """Generate n unique random strings."""
    print(f"Generating {n} synthetic items...")
    # UUIDs are virtually guaranteed to be unique
    return [str(uuid.uuid4()) for _ in range(n)]


def run_all() -> None:
    """Run all tests."""
    
    # Raw token count (no preprocessing) and full unique token list size
    #raw_count = count_raw_tokens()
    #print(f"Raw CSV tokens (no preprocessing): {raw_count}")

    full_words = generate_synthetic_data(100_000)
    #full_words = load_unique_tokens()
    print(f"Full dataset unique tokens: {len(full_words)}")

    print("=" * 60)
    print("Running STANDARD Bloom Filter Test Suite (80/20 split)")
    print("=" * 60)
    print()

    # Build and test the standard Bloom Filter
    bloom, train, test = build_split(full_words)
    
    test_membership(bloom, train)
    test_false_positive_on_heldout(bloom, train, test)
    test_collision_analysis(bloom, train, test)
    show_properties(bloom, train)
    std_metrics = test_performance(bloom, train, test)
    print()
    print("=" * 60)
    print("Running LIGHTWEIGHT Bloom Filter Test Suite (80/20 split)")
    print("=" * 60)
    print()

    # Build the lightweight filter using the same deterministic split
    filter_size = max(1, len(train) * 10)
    light = LightweightBloomFilter(size=filter_size, num_hashes=NUM_HASHES)
    light.update(train)

    test_membership(light, train)
    test_false_positive_on_heldout(light, train, test)
    test_collision_analysis(light, train, test)
    show_properties(light, train)
    light_metrics = test_performance(light, train, test)

    print("=" * 60)
    print("COMPARISON: Performance Summary")
    print("=" * 60)
    compare_performance(std_metrics, light_metrics)
    
    print("=" * 60)
    print("Test suite completed successfully!")
    print("=" * 60)


if __name__ == "__main__":
    run_all()
