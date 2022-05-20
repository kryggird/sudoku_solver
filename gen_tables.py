#!/usr/bin/env python
from math import ceil

def gen_idxs(idx):
    row = (idx // 9)
    col = idx % 9

    row_start = row * 9
    col_start = col

    square_row = (row // 3) * 3
    square_col = (col // 3) * 3
    square_start = square_row * 9 + square_col

    row_idxs = [i for i in range(row_start, row_start + 9) if i != idx]
    col_idxs = [i for i in range(col_start, 81, 9) if i != idx]
    square_idxs = [square_start + shift for shift in [0, 1, 2, 9, 10, 11, 18, 19, 20]]

    return row_idxs + col_idxs + [i for i in square_idxs if i not in row_idxs + col_idxs and i != idx]

def gen_mask(idx, count, alignment=None):
    if alignment is not None:
        aligned_count = int(ceil(count / alignment)) * alignment
    else:
        aligned_count = count
    idxs = gen_idxs(idx)

    elems = [0xFFFF if (i in idxs) else 0 for i in range(aligned_count)]
    return elems


def gen_headers():
    return f"#include <stdint.h>"

def gen_code():
    count = 8 + 8 + 4

    lines = []
    for num in range(81):
        shifts = gen_idxs(num)
        assert len(shifts) == count
        as_str = map("{:3d}".format, shifts)
        inner = ", ".join(as_str)
        row = f"{{ {inner} }}"
        lines.append("    " + row)

    concat = ",\n".join(lines)
    total = f"const int COUNT = {count};\nconst int8_t INDICES[81][{count}] = {{\n{concat}\n}};"
    return total

def gen_mask_code():
    mm_count = int(ceil((81 / 16)))
    aligned_count = mm_count * 16

    lines = []
    for num in range(81):
        masks = gen_mask(num, aligned_count, 16)
        as_str = map("{:3d}".format, masks)
        inner = ", ".join(as_str)
        row = f"{{ {inner} }}"
        lines.append("    " + row)

    concat = ",\n".join(lines)
    total = f"const int MM_COUNT = {mm_count};\n_Alignas(32) const uint16_t MM_INDICES[81][{aligned_count}] = {{\n{concat}\n}};"
    return total


if __name__ == "__main__":
    print(gen_headers())
    print()
    print(gen_code())
    print()
    print(gen_mask_code())
    
