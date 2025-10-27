"""Consolidated Bloom filter test suite.

Performs a deterministic 80/20 split of unique tokens (sorted), builds the
filter with the 80% training set, and runs three tests:

1. Membership test on training set (should be all present)
2. False positive rate on held-out test set (real words not inserted)
3. Collision analysis using simple modifications of held-out words

Filter size is set to 10x the number of training items as requested, and
we use 7 hash functions (Kirsch-Mitzenmacher double hashing).
"""
from __future__ import annotations

from typing import Tuple, List

from .bloom_filter import BloomFilter
from .dataset_utils import load_unique_tokens


NUM_HASHES = 7


def build_split() -> Tuple[BloomFilter, List[str], List[str]]:
    """Create deterministic 80/20 split and build the bloom filter on the train set.

    Returns (bloom, train_list, test_list).
    """
    words = sorted(list(load_unique_tokens()))
    n = len(words)
    split = int(n * 0.8)
    train = words[:split]
    test = words[split:]

    # Filter size = 10 * number of training items
    filter_size = max(1, len(train) * 10)
    bloom = BloomFilter(size=filter_size, num_hashes=NUM_HASHES)
    bloom.update(train)

    return bloom, train, test


def test_membership(bloom: BloomFilter, train: List[str]) -> None:
    print("TEST A: Membership on training set")
    missing = [w for w in train if w not in bloom]
    print(f"  Training items: {len(train)}")
    print(f"  Missing after insertion: {len(missing)} (expected 0)")
    if missing:
        print(f"  Example missing: {missing[:5]}")
    print()


def test_false_positive_on_heldout(bloom: BloomFilter, train: List[str], test: List[str]) -> None:
    print("TEST B: False positive rate on held-out real words")
    # Ensure held-out test words are not in training
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


def test_collision_analysis(bloom: BloomFilter, train: List[str], test: List[str]) -> None:
    print("TEST C: Collision analysis with simple modifications of held-out words")
    sample = test[:500]
    modifications = []
    for word in sample:
        modifications.append(word + "x")
        if len(word) > 1:
            modifications.append(word[:-1] + "z")
        modifications.append("x" + word)

    # Remove any accidental actual words
    modifications = [m for m in modifications if m not in train and m not in test]
    if not modifications:
        print("  No modifications available for testing.")
        return
    false_positives = sum(1 for m in modifications if m in bloom)
    rate = false_positives / len(modifications)
    print(f"  Variants tested: {len(modifications)}")
    print(f"  False positives from variants: {false_positives}")
    print(f"  Collision rate: {rate:.6f} ({rate*100:.4f}%)")
    print()


def show_properties(bloom: BloomFilter, train: List[str]) -> None:
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
    print("Running consolidated Bloom filter test suite (80/20 split)")
    bloom, train, test = build_split()
    test_membership(bloom, train)
    test_false_positive_on_heldout(bloom, train, test)
    test_collision_analysis(bloom, train, test)
    show_properties(bloom, train)


if __name__ == "__main__":
    run_all()
