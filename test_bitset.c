#include <stdio.h> // printf

#include "bitset.h"

void test_xor_tzcnt_roundtrip() {
    for (int idx = 0; idx < 128; ++idx) {
        Bitset bitset;
        bitset.data[0] = 0ul; bitset.data[1] = 0ul;
        xor_bit(&bitset, idx);
        int tzcnt_idx = tzcnt(&bitset);

        if (idx != tzcnt_idx) {
            printf("idx: %d tzcnt: %d\n", idx, tzcnt_idx);
            print_bitset(&bitset);
            exit(1); 
        }
    }
}

void test_xor_tzcnt_exhaust(int set_count) {
    Bitset bitset;
    bitset.data[0] = 0ul; bitset.data[1] = 0ul;
    for (int idx = 0; idx < set_count; ++idx) {
        xor_bit(&bitset, idx);
    }

    uint8_t seen[128] = {0};

    while (test_all(bitset)) {
        int tzcnt_idx = tzcnt(&bitset);
        xor_bit(&bitset, tzcnt_idx);

        if (seen[tzcnt_idx]) {
            printf("tzcnt_idx: %d\n", tzcnt_idx);
            print_bitset(&bitset);
            exit(1);

        }
        ++seen[tzcnt_idx];
    }
}

void test_setmask_tzcnt_roundtrip() {
    for (int idx = 0; idx < 128; ++idx) {
        Bitset bitset;
        bitset.data[0] = 0ul; bitset.data[1] = 0ul;

        int shift = idx / 16;
        uint16_t mask = ((uint16_t) 1) << ((uint16_t) idx % (uint16_t) 16);
        set_aligned_mask(&bitset, mask, shift);
        int tzcnt_idx = tzcnt(&bitset);

        if (idx != tzcnt_idx) {
            printf("idx: %d shift: %d mask %.04X tzcnt: %d\n", idx, shift, (uint32_t) mask, tzcnt_idx);
            print_bitset(&bitset);
            exit(1);
        }
    }
}

int main() {
    test_xor_tzcnt_roundtrip();
    test_xor_tzcnt_exhaust(81);
    test_xor_tzcnt_exhaust(128);
    test_setmask_tzcnt_roundtrip();
}
