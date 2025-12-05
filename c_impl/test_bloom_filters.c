#define _POSIX_C_SOURCE 200809L

#include "bloom_filter.h"
#include "lightweight_bloom_filter.h"

#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <uuid/uuid.h>

// Function pointer for contains adapters: takes a filter type and a string, returns true if present
typedef bool (*contains_fn)(const void *filter, const char *item);

// PerfMetrics - structure to hold performance metrics for benchmarking
typedef struct PerfMetrics
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

// Metric enumeration for output format usage
typedef enum MetricStyle
{
    METRIC_STYLE_DEFAULT,
    METRIC_STYLE_BYTES,
    METRIC_STYLE_MB,
    METRIC_STYLE_PERCENT,
    METRIC_STYLE_TIME,
} MetricStyle;

/*
* Function prototypes/declarations
*/
static char **generate_dataset(size_t n);
static void free_dataset(char **data, size_t n);
static void membership_test(const char *label, const void *filter, contains_fn contains, char *const train[], size_t train_len);
static double false_positive_test(const char *label, const void *filter, contains_fn contains, char *const test[], size_t test_len);
static double collision_test(const char *label, const void *filter, contains_fn contains, char *const test[], size_t test_len);
static void print_filter_properties(const char *label, size_t size_bits, size_t byte_length, uint32_t num_hashes, size_t inserted);
static PerfMetrics benchmark_bloom_filter(char *const train[], size_t train_len, char *const test[], size_t test_len, size_t filter_bits);
static PerfMetrics benchmark_lightweight_filter(char *const train[], size_t train_len, char *const test[], size_t test_len, size_t filter_bits);
static void compare_metrics(const PerfMetrics *std_metrics, const PerfMetrics *light_metrics);
static double now_seconds(void);
static size_t copy_string_with_limit(char *dest, size_t dest_size, const char *source);
static bool append_variant_suffix(char *buffer, size_t buffer_size, const char *source, char suffix);
static bool replace_variant_last_char(char *buffer, size_t buffer_size, const char *source, char replacement);
static bool prefix_variant(char *buffer, size_t buffer_size, char prefix, const char *source);
static void record_variant(const void *filter, contains_fn contains, const char *variant, size_t *variants, size_t *false_positives);
static double time_inserts(void *filter, void (*add)(void *, const char *), char *const items[], size_t count);
static double time_queries(const void *filter, contains_fn contains, char *const items[], size_t count);
static void bloom_add_wrapper(void *filter, const char *item);
static void lbf_add_wrapper(void *filter, const char *item);
static void print_metric_row(const char *name, double std_value, double light_value, const char *diff_text, MetricStyle style);
static char *format_with_commas(size_t num);

/*
* Constants
*/
static const uint32_t kNumHashes = 7u; // In case of standard BF, expand 2 hashes into 7 via double hashing. In case of lightweight BF, just use flip/check 7 bits in a block
static const size_t kDatasetSize = 100000u; // CHANGE SIZE OF DATASET HERE!!
static const uint32_t kTrainPercent = 80u; // Percentage of dataset to use for training (insertion). Remaining percentage used for testing (query).
static const size_t kCollisionSampleLimit = 500u; // Limit on number of test items to use for collision analysis
static const size_t kUuidStringLength = 37u; // 36 chars + null terminator (UUID v4)
static const size_t kVariantBufferSize = 64u; 
static const double kBytesPerMegabyte = 1024.0 * 1024.0; // Conversion factor from bytes to megabytes
static const size_t kBitsPerItem = 10u; // Target bits per item in the Bloom filter (affects size of filter)

// Standard Bloom filter contains adapter
static bool bloom_contains_adapter(const void *filter, const char *item)
{
    return bloom_contains((const BloomFilter *)filter, item);
}

// Lightweight Bloom filter contains adapter
static bool lbf_contains_adapter(const void *filter, const char *item)
{
    return lbf_contains((const LightweightBloomFilter *)filter, item);
}

/*
* Main function - run the benchmarks
*/
int main(void)
{
    printf("Generating %s synthetic items...\n", format_with_commas(kDatasetSize));
    char **dataset = generate_dataset(kDatasetSize);
    if (!dataset)
    {
        fprintf(stderr, "Failed to allocate dataset\n");
        return EXIT_FAILURE;
    }

    const size_t train_len = (size_t)((double)kDatasetSize * (kTrainPercent / 100.0));
    const size_t test_len = kDatasetSize - train_len;
    const size_t train_pct = kTrainPercent;
    const size_t test_pct = 100u - train_pct;
    char *const *train = dataset;
    char *const *test = dataset + train_len;

    const size_t filter_bits = train_len * kBitsPerItem;

    printf("Full dataset unique UUIDs: %s\n\n", format_with_commas(kDatasetSize));

    BloomFilter std_filter;
    if (!bloom_init(&std_filter, filter_bits, kNumHashes, 0u, 0u))
    {
        fprintf(stderr, "Failed to initialize standard Bloom filter\n");
        free_dataset(dataset, kDatasetSize);
        return EXIT_FAILURE;
    }
    for (size_t i = 0; i < train_len; ++i)
    {
        bloom_add(&std_filter, train[i]);
    }

    printf("================================================================\n");
    printf("Running STANDARD Bloom Filter Benchmarks (%zu/%zu split)\n", train_pct, test_pct);
    printf("================================================================\n\n");

    membership_test("STANDARD", &std_filter, bloom_contains_adapter, train, train_len);
    double std_fpr = false_positive_test("STANDARD", &std_filter, bloom_contains_adapter, test, test_len);
    double std_collision_rate = collision_test("STANDARD", &std_filter, bloom_contains_adapter, test, test_len);
    print_filter_properties("STANDARD", std_filter.size_bits, std_filter.byte_length,
                            std_filter.num_hashes, train_len);
    PerfMetrics std_metrics = benchmark_bloom_filter(train, train_len, test, test_len, filter_bits);
    std_metrics.false_positive_rate = std_fpr;
    std_metrics.collision_rate = std_collision_rate;
    std_metrics.filter_bytes = std_filter.byte_length;
    std_metrics.filter_mb = (double)std_filter.byte_length / kBytesPerMegabyte;

    LightweightBloomFilter light_filter;
    if (!lbf_init(&light_filter, filter_bits, kNumHashes, 0u))
    {
        fprintf(stderr, "Failed to initialize lightweight Bloom filter\n");
        bloom_free(&std_filter);
        free_dataset(dataset, kDatasetSize);
        return EXIT_FAILURE;
    }
    for (size_t i = 0; i < train_len; ++i)
    {
        lbf_add(&light_filter, train[i]);
    }

    printf("\n================================================================\n");
    printf("Running LIGHTWEIGHT Bloom Filter Benchmarks (%zu/%zu split)\n", train_pct, test_pct);
    printf("================================================================\n\n");

    membership_test("LIGHTWEIGHT", &light_filter, lbf_contains_adapter, train, train_len);
    double light_fpr = false_positive_test("LIGHTWEIGHT", &light_filter, lbf_contains_adapter, test, test_len);
    double light_collision_rate = collision_test("LIGHTWEIGHT", &light_filter, lbf_contains_adapter, test, test_len);
    const size_t light_bytes = light_filter.word_count * sizeof(uint64_t);
    print_filter_properties("LIGHTWEIGHT", light_filter.size_bits, light_bytes,
                            light_filter.num_hashes, train_len);
    PerfMetrics light_metrics = benchmark_lightweight_filter(train, train_len, test, test_len, filter_bits);
    light_metrics.false_positive_rate = light_fpr;
    light_metrics.collision_rate = light_collision_rate;
    light_metrics.filter_bytes = light_bytes;
    light_metrics.filter_mb = (double)light_bytes / kBytesPerMegabyte;

    printf("===========================================================================\n");
    printf("COMPARISON: Performance Summary (Total Dataset Size: %s) UUIDs\n", format_with_commas(kDatasetSize));
    printf("===========================================================================\n");
    compare_metrics(&std_metrics, &light_metrics);

    bloom_free(&std_filter);
    lbf_free(&light_filter);
    free_dataset(dataset, kDatasetSize);

    return EXIT_SUCCESS;
}

static char **generate_dataset(size_t n)
{
    char **data = malloc(n * sizeof(char *));
    if (!data)
    {
        return NULL;
    }
    for (size_t i = 0; i < n; ++i)
    {
        data[i] = malloc(kUuidStringLength);
        if (!data[i])
        {
            for (size_t freed = 0; freed < i; ++freed)
            {
                free(data[freed]);
            }
            free(data);
            return NULL;
        }
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
                            char *const train[], size_t train_len)
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
                                char *const test[], size_t test_len)
{
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
                           char *const test[], size_t test_len)
{
    const size_t sample = test_len < kCollisionSampleLimit ? test_len : kCollisionSampleLimit;
    size_t variants = 0;
    size_t false_positives = 0;
    char buffer[kVariantBufferSize];

    for (size_t i = 0; i < sample; ++i)
    {
        if (append_variant_suffix(buffer, sizeof(buffer), test[i], 'X'))
        {
            record_variant(filter, contains, buffer, &variants, &false_positives);
        }
        if (replace_variant_last_char(buffer, sizeof(buffer), test[i], 'Z'))
        {
            record_variant(filter, contains, buffer, &variants, &false_positives);
        }
        if (prefix_variant(buffer, sizeof(buffer), 'X', test[i]))
        {
            record_variant(filter, contains, buffer, &variants, &false_positives);
        }
    }

    double rate = variants ? ((double)false_positives / (double)variants) : 0.0;
    printf("TEST C (%s): Collision analysis\n", label);
    printf("  Variants tested: %zu\n", variants);
    printf("  False positives from variants: %zu\n", false_positives);
    printf("  Collision rate: %.6f (%.4f%%)\n\n", rate, rate * 100.0);
    return rate;
}

static void print_filter_properties(const char *label, size_t size_bits, size_t byte_length,
                                    uint32_t num_hashes, size_t inserted)
{
    printf("TEST D (%s): Filter properties\n", label);
    printf("  Filter size (bits): %zu\n", size_bits);
    printf("  Filter size (bytes): %zu\n", byte_length);
    printf("  Filter size (MB): %.2f\n", byte_length / kBytesPerMegabyte);
    printf("  Number of hash functions: %u\n", num_hashes);
    printf("  Words inserted: %zu\n", inserted);
    printf("  Bytes per word: %.4f\n\n", inserted ? (double)byte_length / (double)inserted : 0.0);
}

static PerfMetrics benchmark_bloom_filter(char *const train[], size_t train_len,
                                          char *const test[], size_t test_len, size_t filter_bits)
{
    PerfMetrics metrics = {0};
    BloomFilter filter;
    if (!bloom_init(&filter, filter_bits, kNumHashes, 0u, 0u))
    {
        fprintf(stderr, "Failed to initialize benchmark bloom filter\n");
        return metrics;
    }

    metrics.insert_count = train_len;
    metrics.insert_time = time_inserts(&filter, bloom_add_wrapper, train, train_len);
    metrics.insert_ops_per_sec = metrics.insert_time > 0.0
                                     ? (double)metrics.insert_count / metrics.insert_time
                                     : 0.0;

    metrics.query_count = test_len;
    metrics.query_time = time_queries(&filter, bloom_contains_adapter, test, test_len);
    metrics.query_ops_per_sec = metrics.query_time > 0.0
                                    ? (double)metrics.query_count / metrics.query_time
                                    : 0.0;

    bloom_free(&filter);
    printf("TEST E (STANDARD): Performance Benchmarking\n");
    printf("  - Inserted %zu items in %.5f sec (%.0f ops/sec)\n",
           metrics.insert_count, metrics.insert_time, metrics.insert_ops_per_sec);
    printf("  - Performed %zu queries in %.5f sec (%.0f ops/sec)\n\n",
           metrics.query_count, metrics.query_time, metrics.query_ops_per_sec);
    return metrics;
}

static PerfMetrics benchmark_lightweight_filter(char *const train[], size_t train_len,
                                                char *const test[], size_t test_len, size_t filter_bits)
{
    PerfMetrics metrics = {0};
    LightweightBloomFilter filter;
    if (!lbf_init(&filter, filter_bits, kNumHashes, 0u))
    {
        fprintf(stderr, "Failed to initialize benchmark lightweight filter\n");
        return metrics;
    }

    metrics.insert_count = train_len;
    metrics.insert_time = time_inserts(&filter, lbf_add_wrapper, train, train_len);
    metrics.insert_ops_per_sec = metrics.insert_time > 0.0
                                     ? (double)metrics.insert_count / metrics.insert_time
                                     : 0.0;

    metrics.query_count = test_len;
    metrics.query_time = time_queries(&filter, lbf_contains_adapter, test, test_len);
    metrics.query_ops_per_sec = metrics.query_time > 0.0
                                    ? (double)metrics.query_count / metrics.query_time
                                    : 0.0;

    lbf_free(&filter);
    printf("TEST E (LIGHTWEIGHT): Performance Benchmarking\n");
    printf("  - Inserted %zu items in %.5f sec (%.0f ops/sec)\n",
           metrics.insert_count, metrics.insert_time, metrics.insert_ops_per_sec);
    printf("  - Performed %zu queries in %.5f sec (%.0f ops/sec)\n\n",
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
        MetricStyle style;
    } rows[] = {
        {"Insertion Throughput (ops/sec)", std_metrics->insert_ops_per_sec, light_metrics->insert_ops_per_sec, METRIC_STYLE_DEFAULT},
        {"Insertion Time (sec)", std_metrics->insert_time, light_metrics->insert_time, METRIC_STYLE_TIME},
        {"Query Throughput (ops/sec)", std_metrics->query_ops_per_sec, light_metrics->query_ops_per_sec, METRIC_STYLE_DEFAULT},
        {"Query Time (sec)", std_metrics->query_time, light_metrics->query_time, METRIC_STYLE_TIME},
        {"Insert Count", (double)std_metrics->insert_count, (double)light_metrics->insert_count, METRIC_STYLE_DEFAULT},
        {"Query Count", (double)std_metrics->query_count, (double)light_metrics->query_count, METRIC_STYLE_DEFAULT},
        {"Filter size (bytes)", (double)std_metrics->filter_bytes, (double)light_metrics->filter_bytes, METRIC_STYLE_BYTES},
        {"Filter size (MB)", std_metrics->filter_mb, light_metrics->filter_mb, METRIC_STYLE_MB},
        {"False Positive Rate (%)", std_metrics->false_positive_rate * 100.0, light_metrics->false_positive_rate * 100.0, METRIC_STYLE_PERCENT},
        {"Collision Rate (%)", std_metrics->collision_rate * 100.0, light_metrics->collision_rate * 100.0, METRIC_STYLE_PERCENT},
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

        print_metric_row(rows[i].name, stdv, lightv, diff_s, rows[i].style);
    }
    printf("\n");
}

static size_t copy_string_with_limit(char *dest, size_t dest_size, const char *source)
{
    if (dest_size == 0u)
    {
        return 0u;
    }
    size_t copy_len = strnlen(source, dest_size - 1u);
    memcpy(dest, source, copy_len);
    dest[copy_len] = '\0';
    return copy_len;
}

// Build a variant by appending a suffix without overflowing the buffer.
static bool append_variant_suffix(char *buffer, size_t buffer_size, const char *source, char suffix)
{
    size_t len = copy_string_with_limit(buffer, buffer_size, source);
    if (len + 1u >= buffer_size)
    {
        return false;
    }
    buffer[len] = suffix;
    buffer[len + 1u] = '\0';
    return true;
}

// Build a variant by mutating the last character when possible.
static bool replace_variant_last_char(char *buffer, size_t buffer_size, const char *source, char replacement)
{
    size_t len = copy_string_with_limit(buffer, buffer_size, source);
    if (len == 0u)
    {
        return false;
    }
    buffer[len - 1u] = replacement;
    buffer[len] = '\0';
    return true;
}

// Build a variant by prefixing a character, rejecting truncated outputs.
static bool prefix_variant(char *buffer, size_t buffer_size, char prefix, const char *source)
{
    int written = snprintf(buffer, buffer_size, "%c%s", prefix, source);
    return (written > 0) && ((size_t)written < buffer_size);
}

// Count a variant check and update the false-positive counter if needed.
static void record_variant(const void *filter, contains_fn contains, const char *variant,
                           size_t *variants, size_t *false_positives)
{
    ++(*variants);
    if (contains(filter, variant))
    {
        ++(*false_positives);
    }
}

static double time_inserts(void *filter, void (*add)(void *, const char *), char *const items[], size_t count)
{
    const double start = now_seconds();
    for (size_t i = 0; i < count; ++i)
    {
        add(filter, items[i]);
    }
    return now_seconds() - start;
}

static double time_queries(const void *filter, contains_fn contains, char *const items[], size_t count)
{
    const double start = now_seconds();
    for (size_t i = 0; i < count; ++i)
    {
        (void)contains(filter, items[i]);
    }
    return now_seconds() - start;
}

static void bloom_add_wrapper(void *filter, const char *item)
{
    bloom_add((BloomFilter *)filter, item);
}

static void lbf_add_wrapper(void *filter, const char *item)
{
    lbf_add((LightweightBloomFilter *)filter, item);
}

static void print_metric_row(const char *name, double std_value, double light_value,
                             const char *diff_text, MetricStyle style)
{
    switch (style)
    {
    case METRIC_STYLE_PERCENT:
        printf("%-44s%13.6f%%%17.6f%%%14s\n", name, std_value, light_value, diff_text);
        break;
    case METRIC_STYLE_MB:
        printf("%-44s%14.2f%18.2f%14s\n", name, std_value, light_value, diff_text);
        break;
    case METRIC_STYLE_BYTES:
        printf("%-44s%14.0f%18.0f%14s\n", name, std_value, light_value, diff_text);
        break;
    case METRIC_STYLE_TIME:
        printf("%-44s%14.5f%18.5f%14s\n", name, std_value, light_value, diff_text);
        break;
    default:
        printf("%-44s%14.2f%18.2f%14s\n", name, std_value, light_value, diff_text);
        break;
    }
}

static double now_seconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + ts.tv_nsec / 1e9;
}

static char *format_with_commas(size_t num)
{
    static char buffer[32];
    char temp[32];
    int len = sprintf(temp, "%zu", num);
    int comma_count = (len - 1) / 3;
    int new_len = len + comma_count;
    buffer[new_len] = '\0';
    int j = new_len - 1;
    int count = 0;
    for (int i = len - 1; i >= 0; --i)
    {
        buffer[j--] = temp[i];
        if (++count % 3 == 0 && i > 0)
        {
            buffer[j--] = ',';
        }
    }
    return buffer;
}
