#!/usr/bin/env python

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
    total = f"#include <stdint.h>\n\nconst int COUNT = {count};\nconst int8_t INDICES[81][{count}] = {{\n{concat}\n}};"
    return total

if __name__ == "__main__":
    print(gen_code())
    
