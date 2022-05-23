#include <unistd.h> // read
#include <sys/stat.h> // fstat
#include <fcntl.h> // open, close

#include <stddef.h> // size_t
#include <stdint.h> // uint*_t
#include <stdio.h> // printf
#include <stdlib.h> // calloc

#include <x86intrin.h> // tzcnt, popcnt

#include "bitset.h"
#include "tables.c"
#include "simd.h"

#ifndef DEBUG_VERIFY
    #define DEBUG_VERIFY 0
#endif

const char* TEST_PROBLEM = "004300209005009001070060043006002087190007400050083000600000105003508690042910300";
const char* TEST_SOLUTION = "864371259325849761971265843436192587198657432257483916689734125713528694542916378";

typedef struct Board {
    union {
        uint16_t flags[96];
        __m256i mm_flags[6];
    };
} Board;

typedef struct Solution {
    Board solution;
    int is_solved;
} Solution;

typedef struct State {
    Board current;
    Bitset to_visit;
} State;

typedef struct Stack {
    State* data;
    size_t capacity;
    size_t size;
} Stack;

int exactly_one(int val) {
    return (val != 0) & !(val & (val - 1));
}

uint16_t val_to_mask(int val) {
    return 1u << val;
}

Stack alloc_stack(size_t capacity) {
    size_t raw_size = capacity * sizeof(State);
    size_t alignment = sizeof(__m256i) * 8;
    size_t rounded_size = (raw_size / alignment + 1) * alignment;

    State* data_ptr = aligned_alloc(alignment, rounded_size);

    if (data_ptr == NULL) { 
        exit(1); 
    }
    Stack stack = (Stack) {data_ptr, capacity, 0ul};
    return stack;
}

void stack_push(Stack* stack, State board) {
    if (stack->data == NULL || stack->size >= stack->capacity) {
        exit(1);
    }
    stack->data[stack->size] = board;
    stack->size += 1ul;
}

State stack_pop(Stack* stack) {
    if (stack->data == NULL || stack->size == 0ul) {
        exit(1);
    }
    stack->size -= 1ul;
    return stack->data[stack->size]; // Indexing is 0-based => stack->size is one after the top of the stack
}

int stack_nonempty(Stack* stack) {
    return stack->size >= 1ul;
}

Board make_empty_board() {
    Board board;
    for (int idx = 0; idx < 81; ++idx) {
        board.flags[idx] = 0b0111111111;
    }
    for (int idx = 81; idx < 96; ++idx) {
        board.flags[idx] = 0b1000000000;
    }
    return board;
}

State make_empty_state() {
    State state;
    state.current = make_empty_board();
    state.to_visit = make_empty_bitset();

    for (uint64_t idx = 0; idx < 81; ++idx) {
        xor_bit(&state.to_visit, idx);
    }
    return state;
}

Board make_solution_board(const char* solution) {
    Board board;
    for (int idx = 0; idx < 81; ++idx) {
        int val = solution[idx] - '1';
        board.flags[idx] = 1 << val;
    }
    return board;
}


void mark_false(Board* board, int idx, uint16_t val);
void mark_true(Board* board, int idx, uint16_t val);

// mask is the *true* mask. Aka `1 << val`.
Bitset find_recurse_set(Board* board, int true_cell_idx, uint16_t mask) {
    Bitset recurse_set = make_empty_bitset();

    for (int shift_idx = 0; shift_idx < COUNT; ++shift_idx) {
        int idx = INDICES[true_cell_idx][shift_idx];
        int is_set = board->flags[idx] & mask;
        int flag_andnot_mask = board->flags[idx] & ~mask;

        int bit = (is_set > 0) & exactly_one(flag_andnot_mask);
        if (bit) {
            xor_bit(&recurse_set, idx);
        }
    }

    return recurse_set;
}

// mask is the *true* mask. Aka `1 << val`.
void mark_false_no_recurse(Board* board, int true_cell_idx, uint16_t mask) {
    for (int shift_idx = 0; shift_idx < COUNT; ++shift_idx) {
        int idx = INDICES[true_cell_idx][shift_idx];
        board->flags[idx] = board->flags[idx] & ~mask;
    }
}

// mask is the *true* mask. Aka `1 << val`.
Bitset mark_false_no_recurse_m256(Board* board, int true_cell_idx, uint16_t mask) {
    __m256i MM_ZERO = _mm256_set1_epi16(0);
    __m256i* mm_board_ptr = (__m256i*) &(board->mm_flags);
    __m256i* mm_cell_masks_ptr = (__m256i*) &(MM_INDICES[true_cell_idx]);
    __m256i flag_mask = _mm256_set1_epi16(mask);

    Bitset bitset = make_empty_bitset();
    
    for (int shift_idx = 0; shift_idx < MM_COUNT; ++shift_idx) {
        __m256i mm_flags = _mm256_load_si256(mm_board_ptr + shift_idx);
        __m256i mm_cells = _mm256_load_si256(mm_cell_masks_ptr + shift_idx);
        __m256i masked_cells = _mm256_and_si256(mm_cells, flag_mask);

        __m256i output = _mm256_andnot_si256(masked_cells, mm_flags);
        __m256i is_set = _mm256_cmpgt_epi16(
            _mm256_and_si256(masked_cells, mm_flags), MM_ZERO
        );
        __m256i mm_recurse = _mm256_and_si256(
            is_set, exactly_one_m256(output)
        );

        int recurse_set = movemask_epi16(mm_recurse);

        set_aligned_mask(&bitset, (uint16_t) recurse_set, shift_idx);
        
        // Store
        _mm256_store_si256(mm_board_ptr + shift_idx, output);
    }

    return bitset;
}



// mask is the *true* mask. Aka `1 << val`.
void mark_false(Board* board, int idx, uint16_t mask) {
    //uint16_t mask = 1 << val;
    int is_set = board->flags[idx] & mask;
    board->flags[idx] &= ~mask;

    if (exactly_one(board->flags[idx]) && is_set) {
        //uint16_t new_val = __tzcnt_u32(board->flags[idx]);
        mark_true(board, idx, board->flags[idx]);
    }
}

void mark_true(Board* board, int idx, uint16_t mask) {
    board->flags[idx] &= mask;

    Bitset recurse_set = mark_false_no_recurse_m256(board, idx, mask);

    while (test_all(recurse_set)) {
        int flag_idx = tzcnt(&recurse_set);
        uint16_t new_mask = board->flags[flag_idx];

        xor_bit(&recurse_set, flag_idx);

        Bitset new_bitset = mark_false_no_recurse_m256(board, flag_idx, new_mask);
        recurse_set = or_all(recurse_set, new_bitset);
    }
}

int verify(Board* board) {
    int accumulator = 1;
    for (int idx = 0; idx < 96; ++idx) {
        accumulator &= (board->flags[idx] != 0);
    }
    return accumulator;
}

int verify_m256(Board* board) {
    __m256i MM_ZERO = _mm256_set1_epi16(0);
    __m256i accum = _mm256_set1_epi16(-1);
    __m256i* mm_board_ptr = (__m256i*) &(board->mm_flags);

    for (int idx = 0; idx < MM_COUNT; ++idx) {
        __m256i mm_flags = _mm256_load_si256(mm_board_ptr + idx);
        accum = _mm256_and_si256(accum, _mm256_cmpgt_epi16(mm_flags, MM_ZERO));
    }

    return _mm256_movemask_epi8(accum) == 0xFFFFFFFF;
}

void debug_verify(Board* board) {
    if (!verify(board)) { return; }

    int box_col[9] = {0, 3, 6, 0, 3, 6, 0, 3, 6};
    int box_row[9] = {0, 0, 0, 3, 3, 3, 6, 6, 6};
    int box_shift[9] = {0, 1, 2, 9, 10, 11, 18, 19, 20};

    for (int row_idx = 0; row_idx < 9; ++row_idx) {
        uint8_t seen[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};

        for (int col_idx = 0; col_idx < 9; ++col_idx) {
            int idx = row_idx * 9 + col_idx;
            int val = __tzcnt_u32(board->flags[idx]);
            if (exactly_one(board->flags[idx])) {
                seen[val]++;
                if (seen[val] >= 2) {
                    exit(3);
                }
            }
        }
    }
    
    for (int col_idx = 0; col_idx < 9; ++col_idx) {
        uint8_t seen[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};

        for (int row_idx = 0; row_idx < 9; ++row_idx) {
            int idx = row_idx * 9 + col_idx;
            int val = __tzcnt_u32(board->flags[idx]);
            if (exactly_one(board->flags[idx])) {
                seen[val]++;
                if (seen[val] >= 2) {
                    exit(3);
                }
            }
        }
    }

    for (int box_idx = 0; box_idx < 9; ++box_idx) {
        uint8_t seen[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
        int box_start = box_row[box_idx] * 9 + box_col[box_idx];

        for (int inner_idx = 0; inner_idx < 9; ++inner_idx) {
            int idx = box_start + box_shift[inner_idx];
            int val = __tzcnt_u32(board->flags[idx]);
            if (exactly_one(board->flags[idx])) {
                seen[val]++;
                if (seen[val] >= 2) {
                    exit(3);
                }
            }
        }
    }
}

int is_solution(Board* board) {
    int ret = 1;
    for (int idx = 0; idx < 96; ++idx) {
        ret &= exactly_one(board->flags[idx]);
    }
    return ret;
}

int is_solution_m256(Board* board) {
    __m256i accum = _mm256_set1_epi16(-1);
    __m256i* mm_board_ptr = (__m256i*) &(board->mm_flags);

    for (int idx = 0; idx < MM_COUNT; ++idx) {
        __m256i mm_flags = _mm256_load_si256(mm_board_ptr + idx);
        accum = _mm256_and_si256(accum, exactly_one_m256(mm_flags));
    }

    return _mm256_movemask_epi8(accum) == 0xFFFFFFFF;
}

void print_board(Board board) {
    const char* digits = "123456789";

    for (int row = 0; row < 9; ++row) {
        for (int col = 0; col < 9; ++col) {
            int idx = row * 9 + col;
            int val = __tzcnt_u32(board.flags[idx]);

            if (exactly_one(board.flags[idx]) && val >= 0 && val < 9) {
                printf("%c", digits[val]);
            } else {
                printf(".");
            }
        }
        printf("\n");
    }
}

void print_solution(Solution solution) {
    const char* digits = "123456789";
    if (solution.is_solved) {
        for (int idx = 0; idx < 81; ++idx) {
            int val = __tzcnt_u32(solution.solution.flags[idx]);
            if (val >= 9 || __builtin_popcount(solution.solution.flags[idx]) != 1) { exit(2); }
            printf("%c", digits[val]);
        }
    } else {
        printf("Unsolved!");
    }
    printf("\n");
}

void print_flags(const Board* board) {
    for (int row = 0; row < 9; ++row) {
        for(int col = 0; col < 9; ++col) {
            int idx = row * 9 + col;
            for (int val = 0; val < 9; ++val) {
                char c = ((board->flags[idx] >> val) & 1) ? '.' : ('0' + val);
                printf("%c", c);
            }
            printf(" ");
        }
        printf("\n");
    }
    printf("\n");
}

int find_idx(State* state) {
    Bitset candidates = state->to_visit;
    int argmin = tzcnt(&candidates);
    int min = _mm_popcnt_u32(state->current.flags[argmin]);
    xor_bit(&candidates, argmin);

    while (min > 2 && test_all(candidates)) {
        int idx = tzcnt(&candidates);
        if (idx >= 81) {
            break;
        }
        int popcnt = _mm_popcnt_u32(state->current.flags[idx]);
        xor_bit(&candidates, idx);

        argmin = (popcnt < min) ? idx : argmin;
        min = (popcnt < min) ? popcnt : min;
    }

    return argmin;
}

Solution solve_from_candidates(Stack* stack_ptr) {
    Solution solution = (Solution) {make_empty_board(), 0};

    while(stack_nonempty(stack_ptr)) {
        State state = stack_pop(stack_ptr);
        //printf("Stack popped!\n");

        /* int seen[81] = {0}; */
        /* int depth = 0; */

        while (test_all(state.to_visit)) {
            int idx = find_idx(&state);

            /* ++depth; */
            /* ++seen[idx]; */
            /* if (depth > 100) { */
            /*     exit(99); */
            /* } else if (idx >= 81) { */
            /*     printf("idx: %d flags: %X\n", idx, state.current.flags[idx]); */
            /*     print_bitset(&state.to_visit); */
            /*     exit(100); */
            /* } else if (seen[idx] >= 2) {  */
            /*     exit(101);  */
            /* } */

            if (!verify_m256(&state.current)) {
                //printf("Verify failed!\n");
                break;
            } else if (exactly_one(state.current.flags[idx])) {
                //printf("Nothing to do here!\n");
                if (is_solution_m256(&state.current)) {
                    solution.solution = state.current;
                    solution.is_solved = 1;
                    return solution;
                } else {
                    xor_bit(&state.to_visit, idx);
                    continue;
                }
            } else {
                //printf("Adding new branch!\n");
                State next = state;
                int val = __tzcnt_u32(state.current.flags[idx]);

                mark_false(&next.current, idx, val_to_mask(val));
                stack_push(stack_ptr, next);

                mark_true(&state.current, idx, val_to_mask(val));
                xor_bit(&state.to_visit, idx);
            }
        }

        if (is_solution_m256(&state.current)) {
            solution.solution = state.current;
            solution.is_solved = 1;
            return solution;
        } else {
            continue;
        }
    }

    return solution;
}

Solution solve_one(const char* problem) {
    State state = make_empty_state();

    for (int idx = 0; idx < 81; ++idx) {
        if ((problem[idx] != '0') && (problem[idx] != '.')) {
            int val = problem[idx] - '1';
            
            mark_true(&state.current, idx, val_to_mask(val));
            xor_bit(&state.to_visit, idx);
        }
    }

    Stack stack = alloc_stack(81ul);
    stack_push(&stack, state);

    Solution solution = solve_from_candidates(&stack);
    free(stack.data);

    //printf("%s\n", solution.is_solved ? "Solved!" : "Unsolved!");
    //print_solution(solution);
    //printf("%s\n", TEST_SOLUTION);

    return solution;
}

void solve_from_csv(const char* filename, int has_solution) {
    struct stat statbuf;
    int fd = open(filename, O_RDONLY);
    if (fd == 0) { exit(5); }
    fstat(fd, &statbuf);

    int64_t buffer_size = statbuf.st_size;
    char* buffer = (char*) calloc(sizeof(char), buffer_size);
    read(fd, buffer, buffer_size);
    close(fd);

    char* current = buffer;
    int64_t remaining_size = buffer_size;
    int64_t step = has_solution
                 ? (81 + 1 /* comma */ + 81 + 1 /* newline */)
                 : (81 + 1);

    // Skip lines which are comments
    while ((remaining_size > 0) && *current == '#') {
        ++current;
        --remaining_size;
        while (remaining_size && *current != '\n') {
            ++current;
            --remaining_size;
        }
        ++current;
        --remaining_size;
    }

    while (remaining_size > step) {
        const char* problem_ptr = current;

        current += step;
        remaining_size -= step;

        Solution candidate = solve_one(problem_ptr);

        if (DEBUG_VERIFY >= 1) {
            debug_verify(&candidate.solution);
        }

        if (has_solution) {
            const char* solution_ptr = problem_ptr + 81 + 1 /* comma */;
            Board solution = make_solution_board(solution_ptr);

            int accumulator = 1;
            for (int idx = 0; idx < 81; ++idx) {
                accumulator = accumulator 
                            && (candidate.solution.flags[idx] == solution.flags[idx]);
            }
            if (!accumulator) {
                printf("Error!\n");
                exit(4);
            }
        } else {
            if(!candidate.is_solved) {
                printf("Error!\n");
                exit(4);
            }
        }
    }

    free(buffer);
}

int main(int argc, char *argv[]) {
    for (int idx = 1; idx < argc; ++idx) {
        solve_from_csv(argv[idx], 0);
    }

    /*
    Solution candidate = solve_one(TEST_PROBLEM);
    Board solution = make_solution_board(TEST_SOLUTION);

    int accumulator = 1;
    for (int idx = 0; idx < 81; ++idx) {
        accumulator &= candidate.solution.flags[idx] == solution.flags[idx];
        if(candidate.solution.flags[idx] != solution.flags[idx]) {
            printf("Different %d\n", idx);
        }
    }
    if (accumulator) {
        printf("Solved!\n");
    }
    */
    
    /*
    Board board = make_empty_board();
    printf("flag: %d, count: %d\n", board.flags[0], (int) board.counts[0]);

    mark_true(&board, 0, 1, '1' - '1');
    mark_true(&board, 1, 0, '2' - '1');

    printf("flag: %d, count: %d\n", board.flags[0], (int) board.counts[0]);
    printf("%3d %3d %3d %3d\n%3d %3d %3d %3d\n%3d %3d %3d %3d\n",
            board.flags[0], board.flags[1], board.flags[2], board.flags[3],
            board.flags[9], board.flags[10], board.flags[11], board.flags[12],
            board.flags[18], board.flags[18], board.flags[20], board.flags[21]
          );
    */
}
