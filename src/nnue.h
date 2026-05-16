#pragma once
#include <immintrin.h>
#include <stdalign.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <incbin.h>

// C implementation of the NNUE from the bullet-lib simple.rs examples

#define HIDDEN_SIZE 1024
#define SCALE 400
#define BUCKET_COUNT 8
#define BUCKET_DIV ((32 + BUCKET_COUNT - 1) / BUCKET_COUNT)
#define QA 255
#define QB 64

INCBIN_EXTERN(nnue_bin);

static inline int32_t clamp_mag(int32_t min, int32_t max, int32_t value) {
    if (value < min)
        return min;
    if (value > max)
        return max;
    return value;
}

static inline int32_t screlu(int32_t x) {
    int32_t y = clamp_mag(0, QA, x);
    return y * y;
}

typedef struct s_accumulator {
    int16_t values[HIDDEN_SIZE];
} accumulator_t;

typedef struct __attribute__((aligned(64))) s_nnue {
    accumulator_t feature_weights[768];
    accumulator_t feature_biases;

    int16_t output_weights[BUCKET_COUNT][2 * HIDDEN_SIZE];
    int16_t output_bias[BUCKET_COUNT];
} nnue_t;

static inline accumulator_t accum_init(nnue_t *nnue) {
    return nnue->feature_biases;
}

static inline void accum_add_feat(const nnue_t *nnue, size_t index,
                                  accumulator_t *accum) {
    for (int i = 0; i < HIDDEN_SIZE; i++) {
        accum->values[i] += nnue->feature_weights[index].values[i];
    }
}

static inline void accum_remove_feat(const nnue_t *nnue, size_t index,
                                     accumulator_t *accum) {
    for (int i = 0; i < HIDDEN_SIZE; i++) {
        accum->values[i] -= nnue->feature_weights[index].values[i];
    }
}

static inline size_t get_bucket(size_t piece_count) {
    size_t bucket = (piece_count - 2) / BUCKET_DIV;
    if (bucket >= BUCKET_COUNT)
        return BUCKET_COUNT - 1;
    else
        return bucket;
}

static inline int32_t evaluate_nnue(const nnue_t *nnue, accumulator_t *us,
                                    accumulator_t *them, size_t piece_count) {
    size_t bucket = get_bucket(piece_count);
    int32_t output = 0;

    // for (int i = 0; i < HIDDEN_SIZE; i++) {
    //     output += screlu(us->values[i]) * nnue->output_weights[bucket][i];
    //     output += screlu(them->values[i]) *
    //               nnue->output_weights[bucket][HIDDEN_SIZE + i];
    // }

    // printf("%d\n", output);

    const int16_t *us_values = us->values;
    const int16_t *them_values = them->values;

    const int16_t *us_weights = nnue->output_weights[bucket];
    const int16_t *them_weights = nnue->output_weights[bucket] + HIDDEN_SIZE;

    const __m512i vec_zero = _mm512_setzero_si512();
    const __m512i vec_qa = _mm512_set1_epi16(QA);
    __m512i sum = vec_zero;


    // implementation from chessprogramming wiki

    for (int i = 0; i < HIDDEN_SIZE; i += 32) {
        const __m512i usv = _mm512_load_si512((const __m512i *)(us_values + i));
        const __m512i themv =
            _mm512_load_si512((const __m512i *)(them_values + i));
        const __m512i usw =
            _mm512_load_si512((const __m512i *)(us_weights + i));
        const __m512i themw =
            _mm512_load_si512((const __m512i *)(them_weights + i));

        const __m512i us_clamped =
            _mm512_min_epi16(_mm512_max_epi16(usv, vec_zero), vec_qa);
        const __m512i them_clamped =
            _mm512_min_epi16(_mm512_max_epi16(themv, vec_zero), vec_qa);

        const __m512i us_results =
            _mm512_madd_epi16(_mm512_mullo_epi16(usw, us_clamped), us_clamped);
        const __m512i them_results = _mm512_madd_epi16(
            _mm512_mullo_epi16(themw, them_clamped), them_clamped);

        sum = _mm512_add_epi32(sum, us_results);
        sum = _mm512_add_epi32(sum, them_results);
    }

    output = _mm512_reduce_add_epi32(sum);

    output /= QA;
    output += nnue->output_bias[bucket];
    output *= SCALE;
    output /= (QA * QB);

    return output;
}

extern const nnue_t* NNUE;

static inline int load_nnue() {
    return 0;
}
