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
from pathlib import Path
from typing import Tuple, List, Optional

from .bloom_filter import BloomFilter


NUM_HASHES = 7
DATASET_DIR = Path(__file__).parent.parent / "dataset"


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


def test_membership(bloom: BloomFilter, train: list[str]) -> None:
    """Verify all training items are present in the filter."""
    print("TEST A: Membership on training set")
    missing = [w for w in train if w not in bloom]
    print(f"  Training items: {len(train)}")
    print(f"  Missing after insertion: {len(missing)} (expected 0)")
    if missing:
        print(f"  Example missing: {missing[:5]}")
    print()


def test_false_positive_on_heldout(bloom: BloomFilter, train: list[str], test: list[str]) -> None:
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


def test_collision_analysis(bloom: BloomFilter, train: list[str], test: list[str]) -> None:
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


def show_properties(bloom: BloomFilter, train: list[str]) -> None:
    """Display filter memory and configuration properties."""
    print("TEST D: Filter properties")
    bytes_len = len(bloom.bit_array)
    mb = bytes_len / (1024 * 1024)
    
    print(f"  Filter size (bits): {bloom.size}")
    print(f"  Filter size (bytes): {bytes_len}")
    print(f"  Filter size (MB): {mb:.2f}")
    print(f"  Number of hash functions: {bloom.num_hashes}")
    print(f"  Words inserted: {len(train)}")
    print(f"  Bytes per word: {bytes_len / len(train):.4f}")
    print()


def run_all() -> None:
    """Run all tests."""
    print("=" * 60)
    print("Running Bloom Filter Test Suite (80/20 split)")
    print("=" * 60)
    print()
    
    # Raw token count (no preprocessing) and full unique token list size
    raw_count = count_raw_tokens()
    print(f"Raw CSV tokens (no preprocessing): {raw_count}")

    # Load full unique token list and print its size before splitting
    full_words = load_unique_tokens()
    print(f"Full dataset unique tokens: {len(full_words)}")

    bloom, train, test = build_split(full_words)
    
    test_membership(bloom, train)
    test_false_positive_on_heldout(bloom, train, test)
    test_collision_analysis(bloom, train, test)
    show_properties(bloom, train)
    
    print("=" * 60)
    print("Test suite completed successfully!")
    print("=" * 60)


if __name__ == "__main__":
    run_all()
