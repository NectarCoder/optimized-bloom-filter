"""
Example demonstrating false positive testing with truly added words.

This script shows why the false positive rate can be 0% - with a filter size of
10 million bits and only ~48k inserted words, the filter is relatively sparse.
We also demonstrate various testing methodologies to understand Bloom filter behavior.
"""
"""Deprecated examples module.

This module has been consolidated into `standard_bf/test_suite.py`.
Run the canonical test with:

    python -m standard_bf.test_suite

Running this module directly will print a short pointer and exit.
"""

if __name__ == "__main__":
    print("examples.py is deprecated. Run: python -m standard_bf.test_suite")
    raise SystemExit(0)
