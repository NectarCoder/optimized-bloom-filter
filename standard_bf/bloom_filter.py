"""Minimal Bloom filter implementation using double hashing.

The filter uses two independent hash families (MurmurHash3 via mmh3 and xxHash64)
combined with the Kirsch-Mitzenmacher optimization to produce the configured
number of hash functions.
"""
from __future__ import annotations

from typing import Iterable, Iterator

import mmh3
import xxhash


class BloomFilter:
    """Simple Bloom filter backed by a bytearray bitset."""

    def __init__(self, size: int, num_hashes: int, *, seed1: int = 0, seed2: int = 0) -> None:
        if size <= 0:
            raise ValueError("size must be positive")
        if num_hashes <= 0:
            raise ValueError("num_hashes must be positive")

        self.size = size
        self.num_hashes = num_hashes
        self.seed1 = seed1
        self.seed2 = seed2
        self._bit_array = bytearray((size + 7) // 8)

    def add(self, item: str) -> None:
        """Insert ``item`` into the filter."""
        for bit_index in self._hashes(item):
            byte_index = bit_index >> 3
            mask = 1 << (bit_index & 7)
            self._bit_array[byte_index] |= mask

    def update(self, items: Iterable[str]) -> None:
        """Insert all ``items`` into the filter."""
        for item in items:
            self.add(item)

    def __contains__(self, item: str) -> bool:  # pragma: no cover - simple guard
        for bit_index in self._hashes(item):
            byte_index = bit_index >> 3
            mask = 1 << (bit_index & 7)
            if not (self._bit_array[byte_index] & mask):
                return False
        return True

    def _hashes(self, item: str) -> Iterator[int]:
        data = item.encode("utf-8")
        h1 = mmh3.hash(data, self.seed1, signed=False)
        h2 = xxhash.xxh64(data, seed=self.seed2).intdigest() % self.size
        if h2 == 0:
            h2 = 1  # Ensure progress for the arithmetic progression.

        for i in range(self.num_hashes):
            yield (h1 + i * h2) % self.size

    @property
    def bit_array(self) -> bytearray:
        """Expose the underlying bit array (primarily for inspection)."""
        return self._bit_array
