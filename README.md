# Optimized Bloom Filter

This repository contains a small, focused Bloom filter implementation in Python plus a deterministic test suite that evaluates membership and empirical false-positive rates using the Brown corpus dataset.

Key points
- Bloom filter implemented in `standard_bf/bloom_filter.py` using MurmurHash3 (`mmh3`) and xxHash64 (`xxhash`).
- Hashing uses the Kirsch–Mitzenmacher double-hashing technique to derive k=7 hash values from two base hashes.
- Deterministic 80/20 train/held-out split of the Brown corpus unique tokens is used for reproducible evaluation.
- The consolidated test suite is `standard_bf/test_suite.py`. It sizes the filter to 10× the training-set size by default (per your request).

Quick start (Windows PowerShell)
1. Create and activate a virtual environment and install dependencies (scripts are in `setup_venv_scripts/`):

	./setup_venv.ps1

2. Run the consolidated test suite:

	python -m standard_bf.test_suite

What the test suite does
- Loads normalized unique tokens from `dataset/brown.csv`.
- Sorts tokens deterministically, splits 80% train / 20% test.
- Builds a Bloom filter with size = 10 × len(train) and k = 7.
- Verifies all training items are present, measures empirical false-positive rate on the held-out test set, and reports simple collision statistics and memory usage.

Notes
- Keep `dataset/README.md` (contains dataset notes). Other intermediate doc files were consolidated into this README.
- If you want to experiment with different filter sizes or hash counts, see `standard_bf/test_suite.py` and `standard_bf/bloom_filter.py`.

Contact
If you want further cleanup (archiving deleted files instead of removing them) or additional test metrics (ROC curve, theoretical vs empirical FPR plots), tell me and I will add them.