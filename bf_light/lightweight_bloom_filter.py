"""
Lightweight Bloom Filter implementation (Blocked / Word-Oriented).

Implements the 'One hash variant and word oriented bit array' strategy
outlined in the mid-term report. This reduces memory accesses (cache misses)
to 1 per operation by fitting all k bits into a single 64-bit word.
"""

from __future__ import annotations

import array
from typing import Iterable

import xxhash


class LightweightBloomFilter:
    """
    Blocked Bloom filter backed by an array of 64-bit unsigned integers.

    Strategy:
    1. Hash item once (64-bit).
    2. Use upper bits to select a 'Block' (Word).
    3. Use lower bits to derive k bit positions *within* that Word.
    """

    def __init__(self, size: int, num_hashes: int, seed: int = 0) -> None:
        """Initialize the Lightweight Bloom filter.

        Args:
            size: Target number of bits (will be rounded up to nearest multiple of 64).
            num_hashes: Number of bits to set per item (k).
            seed: Seed for xxHash64.
        """
        if size <= 0:
            raise ValueError("size must be positive")
        if num_hashes <= 0:
            raise ValueError("num_hashes must be positive")

        self.num_hashes = num_hashes
        self.seed = seed

        # Calculate number of 64-bit words needed
        # We use 'Q' for unsigned long long (8 bytes / 64 bits)
        self.num_words = (size + 63) // 64
        self.size = self.num_words * 64
        self._bit_array = array.array("Q", [0] * self.num_words)

    def add(self, item: str) -> None:
        """Insert ``item`` into the filter using a single memory word access."""
        # 1. Single Hash Calculation
        h = xxhash.xxh64(item, seed=self.seed).intdigest()

        # 2. Determine Word (Block) Index
        # We map the hash to a specific word index
        word_index = h % self.num_words

        # 3. Create Mask for k bits inside this word
        # We use the hash 'h' as a seed for a pseudo-random sequence
        # to pick k bits within the 64-bit range.
        mask = 0

        # A lightweight linear congruential generator (LCG) derived from h
        # to pick k positions.
        # Constants: a=6364136223846793005, c=1442695040888963407 (Knuth's MMIX)
        current_h = h
        for _ in range(self.num_hashes):
            # Transformation to pick a bit position 0-63
            # We mix bits to ensure good spread within the word
            current_h = (
                current_h * 6364136223846793005 + 1442695040888963407
            ) & 0xFFFFFFFFFFFFFFFF
            bit_pos = current_h >> 58  # Take top 6 bits (0-63)
            mask |= 1 << bit_pos

        # 4. Single Memory Write (Word Oriented)
        self._bit_array[word_index] |= mask

    def update(self, items: Iterable[str]) -> None:
        """Insert all ``items`` into the filter."""
        for item in items:
            self.add(item)

    def __contains__(self, item: str) -> bool:
        """Check if ``item`` is in the filter."""
        h = xxhash.xxh64(item, seed=self.seed).intdigest()
        word_index = h % self.num_words

        mask = 0
        current_h = h
        for _ in range(self.num_hashes):
            current_h = (
                current_h * 6364136223846793005 + 1442695040888963407
            ) & 0xFFFFFFFFFFFFFFFF
            bit_pos = current_h >> 58
            mask |= 1 << bit_pos

        # 5. Single Memory Read
        return (self._bit_array[word_index] & mask) == mask

    @property
    def bit_array(self) -> array.array:
        """Expose the underlying bit array."""
        return self._bit_array
