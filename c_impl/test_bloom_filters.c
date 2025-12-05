#if !defined(_WIN32)
#define _POSIX_C_SOURCE 199309L
#endif

#include "bloom_filter.h"
#include "lightweight_bloom_filter.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <uuid/uuid.h>

#ifdef _WIN32
#include <windows.h>
#endif

#define NUM_HASHES 7u
#define DATASET_SIZE 10000000u
#define TRAIN_PERCENT 80u

typedef bool (*contains_fn)(const void *filter, const char *item);

typedef struct
{
    size_t insert_count;
    double insert_time;
    double insert_ops_per_sec;
    size_t query_count;
    double query_time;
    double query_ops_per_sec;
    double false_positive_rate;
    double collision_rate;
    size_t filter_bytes;
    double filter_mb;
} PerfMetrics;

static char **generate_dataset(size_t n);
static void free_dataset(char **data, size_t n);
static void membership_test(const char *label, const void *filter, contains_fn contains,
                            char **train, size_t train_len);
static double false_positive_test(const char *label, const void *filter, contains_fn contains,
                                char **train, size_t train_len, char **test, size_t test_len);
static double collision_test(const char *label, const void *filter, contains_fn contains,
                           char **test, size_t test_len);
static void show_properties_standard(const BloomFilter *filter, size_t inserted);
static void show_properties_lightweight(const LightweightBloomFilter *filter, size_t inserted);
static PerfMetrics benchmark_bloom_filter(char **train, size_t train_len,
                                          char **test, size_t test_len, size_t filter_bits);
static PerfMetrics benchmark_lightweight_filter(char **train, size_t train_len,
                                                char **test, size_t test_len, size_t filter_bits);
static void compare_metrics(const PerfMetrics *std_metrics, const PerfMetrics *light_metrics);
static double now_seconds(void);

static bool bloom_contains_adapter(const void *filter, const char *item)
{
    return bloom_contains((const BloomFilter *)filter, item);
}

static bool lbf_contains_adapter(const void *filter, const char *item)
{
    return lbf_contains((const LightweightBloomFilter *)filter, item);
}

int main(void)
{
    printf("Generating %zu synthetic items...\n", (size_t)DATASET_SIZE);
    char **dataset = generate_dataset(DATASET_SIZE);
    if (!dataset)
    {
        fprintf(stderr, "Failed to allocate dataset\n");
        return EXIT_FAILURE;
    }

    const size_t train_len = (size_t)((double)DATASET_SIZE * (TRAIN_PERCENT / 100.0));
    const size_t test_len = DATASET_SIZE - train_len;
    const size_t train_pct = (size_t)TRAIN_PERCENT;
    const size_t test_pct = 100u - train_pct;
    char **train = dataset;
    char **test = dataset + train_len;

    const size_t filter_bits = train_len * 10u;

    printf("Full dataset unique tokens: %zu\n\n", (size_t)DATASET_SIZE);

    BloomFilter std_filter;
    if (!bloom_init(&std_filter, filter_bits, NUM_HASHES, 0u, 0u))
    {
        fprintf(stderr, "Failed to initialize standard Bloom filter\n");
        free_dataset(dataset, DATASET_SIZE);
        return EXIT_FAILURE;
    }
    for (size_t i = 0; i < train_len; ++i)
    {
        bloom_add(&std_filter, train[i]);
    }

    printf("============================================================\n");
    printf("Running STANDARD Bloom Filter Test Suite (%zu/%zu split)\n", train_pct, test_pct);
    printf("============================================================\n\n");

    membership_test("STANDARD", &std_filter, bloom_contains_adapter, train, train_len);
    double std_fpr = false_positive_test("STANDARD", &std_filter, bloom_contains_adapter, train, train_len, test, test_len);
    double std_collision_rate = collision_test("STANDARD", &std_filter, bloom_contains_adapter, test, test_len);
    show_properties_standard(&std_filter, train_len);
    PerfMetrics std_metrics = benchmark_bloom_filter(train, train_len, test, test_len, filter_bits);
    std_metrics.false_positive_rate = std_fpr;
    std_metrics.collision_rate = std_collision_rate;
    std_metrics.filter_bytes = std_filter.byte_length;
    std_metrics.filter_mb = (double)std_filter.byte_length / (1024.0 * 1024.0);

    LightweightBloomFilter light_filter;
    if (!lbf_init(&light_filter, filter_bits, NUM_HASHES, 0u))
    {
        fprintf(stderr, "Failed to initialize lightweight Bloom filter\n");
        bloom_free(&std_filter);
        free_dataset(dataset, DATASET_SIZE);
        return EXIT_FAILURE;
    }
    for (size_t i = 0; i < train_len; ++i)
    {
        lbf_add(&light_filter, train[i]);
    }

    printf("\n============================================================\n");
    printf("Running LIGHTWEIGHT Bloom Filter Test Suite (%zu/%zu split)\n", train_pct, test_pct);
    printf("============================================================\n\n");

    membership_test("LIGHTWEIGHT", &light_filter, lbf_contains_adapter, train, train_len);
    double light_fpr = false_positive_test("LIGHTWEIGHT", &light_filter, lbf_contains_adapter, train, train_len, test, test_len);
    double light_collision_rate = collision_test("LIGHTWEIGHT", &light_filter, lbf_contains_adapter, test, test_len);
    show_properties_lightweight(&light_filter, train_len);
    PerfMetrics light_metrics = benchmark_lightweight_filter(train, train_len, test, test_len, filter_bits);
    light_metrics.false_positive_rate = light_fpr;
    light_metrics.collision_rate = light_collision_rate;
    const size_t light_bytes = light_filter.word_count * sizeof(uint64_t);
    light_metrics.filter_bytes = light_bytes;
    light_metrics.filter_mb = (double)light_bytes / (1024.0 * 1024.0);

    printf("============================================================\n");
    printf("COMPARISON: Performance Summary\n");
    printf("============================================================\n");
    compare_metrics(&std_metrics, &light_metrics);

    bloom_free(&std_filter);
    lbf_free(&light_filter);
    free_dataset(dataset, DATASET_SIZE);

    return EXIT_SUCCESS;
}

static char **generate_dataset(size_t n)
{
    char **data = (char **)malloc(n * sizeof(char *));
    if (!data)
    {
        return NULL;
    }
    for (size_t i = 0; i < n; ++i)
    {
        /* UUIDs require 36 characters plus null-terminator */
        data[i] = (char *)malloc(37u);
        if (!data[i])
        {
            for (size_t j = 0; j < i; ++j)
            {
                free(data[j]);
            }
            free(data);
            return NULL;
        }
        /* Generate a UUID string for each dataset item */
        uuid_t uuid;
        uuid_generate(uuid);
        uuid_unparse_lower(uuid, data[i]);
    }
    return data;
}

static void free_dataset(char **data, size_t n)
{
    if (!data)
    {
        return;
    }
    for (size_t i = 0; i < n; ++i)
    {
        free(data[i]);
    }
    free(data);
}

static void membership_test(const char *label, const void *filter, contains_fn contains,
                            char **train, size_t train_len)
{
    size_t missing = 0;
    for (size_t i = 0; i < train_len; ++i)
    {
        if (!contains(filter, train[i]))
        {
            ++missing;
        }
    }
    printf("TEST A (%s): Membership on training set\n", label);
    printf("  Training items: %zu\n", train_len);
    printf("  Missing after insertion: %zu (expected 0)\n\n", missing);
}

static double false_positive_test(const char *label, const void *filter, contains_fn contains,
                                char **train, size_t train_len, char **test, size_t test_len)
{
    (void)train;
    (void)train_len;
    size_t false_positives = 0;
    for (size_t i = 0; i < test_len; ++i)
    {
        if (contains(filter, test[i]))
        {
            ++false_positives;
        }
    }
    double fpr = test_len ? ((double)false_positives / (double)test_len) : 0.0;
    printf("TEST B (%s): False positive rate on held-out real words\n", label);
    printf("  Held-out words: %zu\n", test_len);
    printf("  False positives: %zu\n", false_positives);
    printf("  Empirical FPR: %.6f (%.4f%%)\n\n", fpr, fpr * 100.0);
    return fpr;
}

static double collision_test(const char *label, const void *filter, contains_fn contains,
                           char **test, size_t test_len)
{
    const size_t sample = test_len < 500u ? test_len : 500u;
    size_t variants = 0;
    size_t false_positives = 0;

    for (size_t i = 0; i < sample; ++i)
    {
        char buffer[64];

        snprintf(buffer, sizeof(buffer), "%sX", test[i]);
        ++variants;
        if (contains(filter, buffer))
        {
            ++false_positives;
        }

        size_t len = strlen(test[i]);
        if (len > 1u)
        {
            strncpy(buffer, test[i], sizeof(buffer) - 1u);
            buffer[len - 1u] = 'Z';
            buffer[len] = '\0';
            ++variants;
            if (contains(filter, buffer))
            {
                ++false_positives;
            }
        }

        snprintf(buffer, sizeof(buffer), "X%s", test[i]);
        ++variants;
        if (contains(filter, buffer))
        {
            ++false_positives;
        }
    }

    double rate = variants ? ((double)false_positives / (double)variants) : 0.0;
    printf("TEST C (%s): Collision analysis\n", label);
    printf("  Variants tested: %zu\n", variants);
    printf("  False positives from variants: %zu\n", false_positives);
    printf("  Collision rate: %.6f (%.4f%%)\n\n", rate, rate * 100.0);
    return rate;
}

static void show_properties_standard(const BloomFilter *filter, size_t inserted)
{
    printf("TEST D (STANDARD): Filter properties\n");
    printf("  Filter size (bits): %zu\n", filter->size_bits);
    printf("  Filter size (bytes): %zu\n", filter->byte_length);
    printf("  Filter size (MB): %.2f\n", filter->byte_length / (1024.0 * 1024.0));
    printf("  Number of hash functions: %u\n", filter->num_hashes);
    printf("  Words inserted: %zu\n", inserted);
    printf("  Bytes per word: %.4f\n\n", inserted ? (double)filter->byte_length / (double)inserted : 0.0);
}

static void show_properties_lightweight(const LightweightBloomFilter *filter, size_t inserted)
{
    const size_t bytes_len = filter->word_count * sizeof(uint64_t);
    printf("TEST D (LIGHTWEIGHT): Filter properties\n");
    printf("  Filter size (bits): %zu\n", filter->size_bits);
    printf("  Filter size (bytes): %zu\n", bytes_len);
    printf("  Filter size (MB): %.2f\n", bytes_len / (1024.0 * 1024.0));
    printf("  Number of hash functions: %u\n", filter->num_hashes);
    printf("  Words inserted: %zu\n", inserted);
    printf("  Bytes per word: %.4f\n\n", inserted ? (double)bytes_len / (double)inserted : 0.0);
}

static PerfMetrics benchmark_bloom_filter(char **train, size_t train_len,
                                          char **test, size_t test_len, size_t filter_bits)
{
    PerfMetrics metrics = {0};
    BloomFilter filter;
    if (!bloom_init(&filter, filter_bits, NUM_HASHES, 0u, 0u))
    {
        fprintf(stderr, "Failed to initialize benchmark bloom filter\n");
        return metrics;
    }

    double start = now_seconds();
    for (size_t i = 0; i < train_len; ++i)
    {
        bloom_add(&filter, train[i]);
    }
    double end = now_seconds();
    metrics.insert_count = train_len;
    metrics.insert_time = end - start;
    metrics.insert_ops_per_sec = metrics.insert_time > 0.0
                                     ? (double)metrics.insert_count / metrics.insert_time
                                     : 0.0;

    const size_t query_count = test_len;
    start = now_seconds();
    for (size_t i = 0; i < query_count; ++i)
    {
        const char *token = test[i];
        (void)bloom_contains(&filter, token);
    }
    end = now_seconds();
    metrics.query_count = query_count;
    metrics.query_time = end - start;
    metrics.query_ops_per_sec = metrics.query_time > 0.0
                                    ? (double)metrics.query_count / metrics.query_time
                                    : 0.0;

    bloom_free(&filter);
    printf("TEST E (STANDARD): Performance Benchmarking\n");
    printf("  - Inserted %zu items in %.4f sec (%.0f ops/sec)\n",
           metrics.insert_count, metrics.insert_time, metrics.insert_ops_per_sec);
    printf("  - Performed %zu queries in %.4f sec (%.0f ops/sec)\n\n",
           metrics.query_count, metrics.query_time, metrics.query_ops_per_sec);
    return metrics;
}

static PerfMetrics benchmark_lightweight_filter(char **train, size_t train_len,
                                                char **test, size_t test_len, size_t filter_bits)
{
    PerfMetrics metrics = {0};
    LightweightBloomFilter filter;
    if (!lbf_init(&filter, filter_bits, NUM_HASHES, 0u))
    {
        fprintf(stderr, "Failed to initialize benchmark lightweight filter\n");
        return metrics;
    }

    double start = now_seconds();
    for (size_t i = 0; i < train_len; ++i)
    {
        lbf_add(&filter, train[i]);
    }
    double end = now_seconds();
    metrics.insert_count = train_len;
    metrics.insert_time = end - start;
    metrics.insert_ops_per_sec = metrics.insert_time > 0.0
                                     ? (double)metrics.insert_count / metrics.insert_time
                                     : 0.0;

    const size_t query_count = test_len;
    start = now_seconds();
    for (size_t i = 0; i < query_count; ++i)
    {
        const char *token = test[i];
        (void)lbf_contains(&filter, token);
    }
    end = now_seconds();
    metrics.query_count = query_count;
    metrics.query_time = end - start;
    metrics.query_ops_per_sec = metrics.query_time > 0.0
                                    ? (double)metrics.query_count / metrics.query_time
                                    : 0.0;

    lbf_free(&filter);
    printf("TEST E (LIGHTWEIGHT): Performance Benchmarking\n");
    printf("  - Inserted %zu items in %.4f sec (%.0f ops/sec)\n",
           metrics.insert_count, metrics.insert_time, metrics.insert_ops_per_sec);
    printf("  - Performed %zu queries in %.4f sec (%.0f ops/sec)\n\n",
           metrics.query_count, metrics.query_time, metrics.query_ops_per_sec);
    return metrics;
}

static void compare_metrics(const PerfMetrics *std_metrics, const PerfMetrics *light_metrics)
{
    printf("Metric                                             Standard       Lightweight      Diff (%%)\n");
    printf("--------------------------------------------------------------------------------------------\n");

    const struct
    {
        const char *name;
        double std_value;
        double light_value;
    } rows[] = {
        {"Insertion Throughput (ops/sec)", std_metrics->insert_ops_per_sec, light_metrics->insert_ops_per_sec},
        {"Insertion Time (s)", std_metrics->insert_time, light_metrics->insert_time},
        {"Query Throughput (ops/sec)", std_metrics->query_ops_per_sec, light_metrics->query_ops_per_sec},
        {"Query Time (s)", std_metrics->query_time, light_metrics->query_time},
        {"Insert Count", (double)std_metrics->insert_count, (double)light_metrics->insert_count},
        {"Query Count", (double)std_metrics->query_count, (double)light_metrics->query_count},
        {"Filter size (bytes)", (double)std_metrics->filter_bytes, (double)light_metrics->filter_bytes},
        {"Filter size (MB)", std_metrics->filter_mb, light_metrics->filter_mb},
        {"False Positive Rate (%)", std_metrics->false_positive_rate * 100.0, light_metrics->false_positive_rate * 100.0},
        {"Collision Rate (%)", std_metrics->collision_rate * 100.0, light_metrics->collision_rate * 100.0},
    };
    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i)
    {
        double stdv = rows[i].std_value;
        double lightv = rows[i].light_value;
        double diff = 0.0;
        bool div_by_zero = false;
        if (fabs(stdv) < 1e-12)
        {
            if (fabs(lightv) < 1e-12)
            {
                diff = 0.0;
            }
            else
            {
                div_by_zero = true;
            }
        }
        else
        {
            diff = ((lightv - stdv) / stdv) * 100.0;
        }

        char diff_s[32];
        if (div_by_zero)
        {
            snprintf(diff_s, sizeof(diff_s), "+Inf%%");
        }
        else if (fabs(diff) < 1e-9)
        {
            snprintf(diff_s, sizeof(diff_s), "~0.00%%");
        }
        else if (diff > 0.0)
        {
            snprintf(diff_s, sizeof(diff_s), "+%.2f%%", diff);
        }
        else
        {
            snprintf(diff_s, sizeof(diff_s), "%.2f%%", diff);
        }

        /* Print values with context-specific formatting */
        if (strstr(rows[i].name, "Rate") != NULL)
        {
            /* percentage rate format: numeric widths minus 1 to accommodate trailing %% */
            printf("%-44s%13.6f%%%17.6f%%%14s\n",
                   rows[i].name, stdv, lightv, diff_s);
        }
        else if (strstr(rows[i].name, "Filter size (MB)") != NULL)
        {
            /* MB with 2 decimals */
            printf("%-44s%14.2f%18.2f%14s\n",
                   rows[i].name, stdv, lightv, diff_s);
        }
        else if (strstr(rows[i].name, "Filter size (bytes)") != NULL)
        {
            /* bytes as integer */
            printf("%-44s%14.0f%18.0f%14s\n",
                   rows[i].name, stdv, lightv, diff_s);
        }
        else
        {
            printf("%-44s%14.2f%18.2f%14s\n",
                   rows[i].name, stdv, lightv, diff_s);
        }
    }
    printf("\n");
}

static double now_seconds(void)
{
#ifdef _WIN32
    static LARGE_INTEGER freq = {0};
    LARGE_INTEGER counter;
    if (freq.QuadPart == 0)
    {
        QueryPerformanceFrequency(&freq);
    }
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + ts.tv_nsec / 1e9;
#endif
}
