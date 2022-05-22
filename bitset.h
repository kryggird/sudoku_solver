#include <stdint.h> // uint64_t
#include <stdio.h> // printf

#include <x86intrin.h> // tzcnt

typedef struct {
    _Alignas(16) union {
        uint64_t data[2];
        __m128i mm_data;
    };
} Bitset;

int tzcnt(Bitset* bitset) {
    int hi = __tzcnt_u64(bitset->data[0]) + 64;
    int lo = __tzcnt_u64(bitset->data[1]);

    return bitset->data[1] ? lo : hi;
}

void set_aligned_mask(Bitset* bitset, uint16_t mask, uint64_t shift) {
    int idx = shift < 4;
    bitset->data[idx] |= ((uint64_t) mask) << ((shift % 4ul) * 16ul);
}

void xor_bit(Bitset* bitset, uint64_t n) {
    int idx = n < 64ul;
    uint64_t shift = idx ? n : n - 64ul;
    if (shift > 64ul) {
        exit(99);
    }
    bitset->data[idx] ^= 1ul << shift;
}

void xor_bit_m128(Bitset* bitset, uint64_t n) {
    int idx = n < 64ul;
    uint64_t shift = idx ? n - 64ul : n;
    uint64_t data = 1ul << shift;
    if (idx) {
        bitset->mm_data = _mm_insert_epi64(bitset->mm_data, data, 1);
    } else {
        bitset->mm_data = _mm_insert_epi64(bitset->mm_data, data, 0);
    }
}

Bitset or_all(Bitset lhs, Bitset rhs) {
    return (Bitset) {
        //.data = {lhs.data[0] | rhs.data[0], lhs.data[1] | rhs.data[1] }
        .mm_data = _mm_or_si128(lhs.mm_data, rhs.mm_data)
    };
}

uint64_t test_all(Bitset bitset) {
    return bitset.data[0] | bitset.data[1];
    //return !_mm_test_all_zeros(bitset.mm_data, _mm_set1_epi32(0));
}

void print_bitset(Bitset* bitset) {
    volatile int ignore_this = 0;

    for (int idx = 0; idx < 81; ++idx) {
        if ((idx > 0) && (idx % 9 == 0)) { printf("\n"); }

        uint64_t data = (idx >= 64) ? bitset->data[1] : bitset->data[0];
        int inner_idx = idx % 64;
        int bit = (data & (1ul << inner_idx)) > 0;
        printf("%c ", bit ? '1' : '.');
        if (bit) {
            ignore_this = 1 + 2; // Do nothing
        }
    }
    printf("\n");
}
