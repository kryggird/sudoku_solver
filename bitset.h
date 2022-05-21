#include <stdint.h> // uint64_t
#include <stdio.h> // printf

#include <x86intrin.h> // tzcnt

typedef struct {
    uint64_t data[2];
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

uint64_t test_all(Bitset* bitset) {
    return bitset->data[0] | bitset->data[1];
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
