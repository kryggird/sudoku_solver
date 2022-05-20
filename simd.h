#include <stdint.h>

#include <x86intrin.h>

__m256i exactly_one_m256(__m256i val) {
    __m256i ZERO = _mm256_set1_epi16(0);
    __m256i at_least_two = _mm256_and_si256(
        val, _mm256_sub_epi16(val, _mm256_set1_epi16(1))
    );

    return _mm256_and_si256(
        _mm256_cmpgt_epi16(val, ZERO),
        _mm256_cmpeq_epi16(at_least_two, ZERO)
    );
}

int movemask_epi16(__m256i val) {
    const uint32_t PEXT_MASK = 0b10101010101010101010101010101010;
    return _pext_u32(_mm256_movemask_epi8(val), PEXT_MASK);
}

int test_all_zeros( __m256i x ) {
    return _mm256_testz_si256( x, x );
}
