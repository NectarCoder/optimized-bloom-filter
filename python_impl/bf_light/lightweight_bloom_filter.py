"""Cache-friendly Bloom filter optimized for low-power devices.

This implementation follows the "blocked" Bloom filter approach described in the
project notes: a single 64-bit hash locates both the machine word (block) and
the bit offsets to touch. All `k` bits for an element live inside one 64-bit
word which dramatically improves cache locality and reduces the number of
memory fetches per operation.

Key design choices:
* **Single hash** via xxHash64. The upper/lower halves of the 64-bit digest are
  re-used (Kirsch-Mitzenmacher style) so we only pay for one hash call.
* **Power-of-two word array** so block selection is just bit masking instead of
  costly modulo operations – an important optimization for MCUs without fast
  hardware division.
* **Word-oriented storage** using ``array('Q')`` (unsigned 64-bit) to align with
  CPU word writes/reads.
"""

from __future__ import annotations

from array import array
from typing import Iterable, Iterator, Tuple

import xxhash


def _next_power_of_two(value: int) -> int:
	"""Return the next power of two >= value."""

	if value <= 1:
		return 1
	return 1 << (value - 1).bit_length()


class LightweightBloomFilter:
	"""Blocked Bloom filter with single-hash, word-oriented bitset."""

	__slots__ = (
		"size",
		"num_hashes",
		"seed",
		"_word_count",
		"_word_mask",
		"_block_bits",
		"_bit_array",
	)

	_SM64_GAMMA = 0x9E3779B97F4A7C15
	_SM64_M1 = 0xBF58476D1CE4E5B9
	_SM64_M2 = 0x94D049BB133111EB
	_MASK64 = (1 << 64) - 1

	def __init__(self, size: int, num_hashes: int, *, seed: int = 0) -> None:
		if size <= 0:
			raise ValueError("size must be positive")
		if num_hashes <= 0:
			raise ValueError("num_hashes must be positive")

		requested_words = max(1, (size + 63) // 64)
		self._word_count = _next_power_of_two(requested_words)
		self.size = self._word_count * 64  # report actual bit capacity
		self.num_hashes = num_hashes
		self.seed = seed
		self._word_mask = self._word_count - 1
		self._block_bits = (self._word_count.bit_length() - 1) if self._word_count > 1 else 0
		self._bit_array = array("Q", [0] * self._word_count)

	def add(self, item: str) -> None:
		"""Insert ``item`` using a single hashed block."""

		block_index, state = self._block_index_and_state(item)
		word = self._bit_array[block_index]
		for bit_pos in self._bit_positions(state):
			word |= 1 << bit_pos
		self._bit_array[block_index] = word

	def update(self, items: Iterable[str]) -> None:
		"""Insert each item from ``items``."""

		for item in items:
			self.add(item)

	def __contains__(self, item: str) -> bool:
		"""Return True if ``item`` may be present, False if definitely absent."""

		block_index, state = self._block_index_and_state(item)
		word = self._bit_array[block_index]
		for bit_pos in self._bit_positions(state):
			if not (word & (1 << bit_pos)):
				return False
		return True

	def _block_index_and_state(self, item: str) -> Tuple[int, int]:
		"""Return block index plus the 64-bit hash state."""

		data = item.encode("utf-8")
		digest = xxhash.xxh64(data, seed=self.seed).intdigest()
		if self._block_bits:
			block_index = (digest >> (64 - self._block_bits)) & self._word_mask
		else:
			block_index = 0
		return block_index, digest & self._MASK64

	def _bit_positions(self, state: int) -> Iterator[int]:
		"""Yield deterministic bit positions inside a 64-bit block."""

		x = state & self._MASK64
		for _ in range(self.num_hashes):
			x = (x + self._SM64_GAMMA) & self._MASK64
			z = x
			z = (z ^ (z >> 30)) * self._SM64_M1 & self._MASK64
			z = (z ^ (z >> 27)) * self._SM64_M2 & self._MASK64
			z ^= z >> 31
			yield z & 63

	@property
	def bit_array(self) -> array:
		"""Expose the internal array('Q') for inspection/metrics."""

		return self._bit_array
