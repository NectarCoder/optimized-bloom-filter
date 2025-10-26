# Brown Corpus Dataset for Bloom Filter Implementation

## Dataset Overview

The Brown Corpus is a renowned dataset in computational linguistics and natural language processing. Key characteristics include:

- Contains approximately 1 million words
- Consists of 500 English text samples (~2,000 words each)
- Organized into 15 categories (press, fiction, religion, government, science, etc.)
- Categories are aggregated into 5 major labels
- Well-balanced and diverse dataset

## Project Purpose

This dataset is being used to implement and test Bloom filter data structure, chosen for its:
- Manageable size for computation
- Real-world text data
- Suitability for word-based operations
- Ideal for testing membership queries and false positive rates

## Data Preprocessing Steps

1. Load the corpus using NLTK
2. Text normalization:
   - Convert all words to lowercase
   - Remove punctuation and non-alphanumeric characters
3. Dataset preparation:
   - Create insertion set (I) from unique words
   - Generate query set (Q) with random/unseen terms

## Performance Analysis

The preprocessed Brown Corpus enables testing of:
- Membership testing accuracy
- False positive characteristics
- Space consumption metrics
- Time-to-complete measurements

This dataset provides a practical benchmark for evaluating Bloom filter performance in real-world language processing applications like dictionary searches and content screening.

## Repository Structure
```
.
├── brown/           # Raw corpus files
├── brown.csv        # Processed data
├── cats.csv         # Category information
└── brown-meta.json  # Metadata
```
