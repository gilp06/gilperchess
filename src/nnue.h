#pragma once
#include <stdalign.h>
#include <stdint.h>
#include <stdio.h>

// C implementation of the NNUE from the bullet-lib simple.rs examples

#define HIDDEN_SIZE 64
#define SCALE 400
#define QA 255
#define QB 64

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

typedef struct s_nnue {
    accumulator_t feature_weights[768];
    accumulator_t feature_biases;

    int16_t output_weights[2 * HIDDEN_SIZE];
    int16_t output_bias;
} nnue_t;

static inline accumulator_t accum_init(nnue_t *nnue) {
    return nnue->feature_biases;
}

static inline void accum_add_feat(nnue_t *nnue, size_t index,
                                  accumulator_t *accum) {
    for (int i = 0; i < HIDDEN_SIZE; i++) {
        accum->values[i] += nnue->feature_weights[index].values[i];
    }
}

static inline void accum_remove_feat(nnue_t *nnue, size_t index,
                                     accumulator_t *accum) {
    for (int i = 0; i < HIDDEN_SIZE; i++) {
        accum->values[i] -= nnue->feature_weights[index].values[i];
    }
}

static inline int32_t evaluate_nnue(nnue_t *nnue, accumulator_t *us,
                                    accumulator_t *them) {
    int32_t output = 0;

    for (int i = 0; i < HIDDEN_SIZE; i++) {
        output += screlu(us->values[i]) * nnue->output_weights[i];
        output += screlu(them->values[i]) * nnue->output_weights[HIDDEN_SIZE + i];
    }

    // printf("%d\n", output);
    
    output /= QA;
    output += nnue->output_bias;
    output *= SCALE;
    output /= (QA * QB);

    return output;
}

extern alignas(64) nnue_t NNUE;

static inline int load_nnue(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f)
        return 0;

    size_t n = fread(&NNUE, 1, sizeof(NNUE), f);
    fclose(f);

    return n == sizeof(NNUE);
}
