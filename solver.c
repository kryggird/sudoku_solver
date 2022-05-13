#include <unistd.h> // read
#include <sys/stat.h> // fstat
#include <fcntl.h> // open, close

#include <stddef.h> // size_t
#include <stdint.h> // uint*_t
#include <stdio.h> // printf
#include <stdlib.h> // calloc

#include <x86intrin.h> // tzcnt

#ifndef DEBUG_VERIFY
    #define DEBUG_VERIFY 0
#endif

const char* TEST_PROBLEM = "004300209005009001070060043006002087190007400050083000600000105003508690042910300";
const char* TEST_SOLUTION = "864371259325849761971265843436192587198657432257483916689734125713528694542916378";

typedef struct Board {
    uint16_t flags[81];    
    uint16_t counts[81];
} Board;

typedef struct Solution {
    Board solution;
    int is_solved;
} Solution;

typedef struct State {
    Board current;
    int idx;
} State;

typedef struct Stack {
    State* data;
    size_t capacity;
    size_t size;
} Stack;

Stack alloc_stack(size_t capacity) {
    State* data_ptr = calloc(capacity, sizeof(State));
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
        board.flags[idx] = 0b111111111;
        board.counts[idx] = 9;
    }
    return board;
}

Board make_solution_board(const char* solution) {
    Board board;
    for (int idx = 0; idx < 81; ++idx) {
        int val = solution[idx] - '1';
        board.flags[idx] = 1 << val;
        board.counts[idx] = 1;
    }
    return board;
}


void mark_false(Board* board, int row, int col, uint16_t val);
void mark_true(Board* board, int row, int col, uint16_t val);

void mark_false(Board* board, int row, int col, uint16_t val) {
    int idx = row * 9 + col;
    uint16_t mask = 1 << val;
    int is_set = board->flags[idx] & mask;
    board->counts[idx] -=  is_set ? 1 : 0;
    board->flags[idx] &= ~mask;

    if (board->counts[idx] == 1 && is_set) {
        uint16_t new_val = __tzcnt_u32(board->flags[idx]);
        mark_true(board, row, col, new_val);
    }
}

void mark_true(Board* board, int row, int col, uint16_t val) {
    int idx = row * 9 + col;
    uint16_t mask = 1 << val;
    board->flags[idx] &= mask;
    board->counts[idx] = (board->flags[idx] == 0) ? 0 : 1;

    for (int shift_idx = 0; shift_idx < 9; ++shift_idx) {
        if (shift_idx != col) {
            mark_false(board, row, shift_idx, val);
        }
        if (shift_idx != row) {
            mark_false(board, shift_idx, col, val);
        }
    }

    int square_row = 3 * (row / 3);
    int square_col = 3 * (col / 3);
    for (int col_idx = 0; col_idx < 3; ++col_idx) {
        for (int row_idx = 0; row_idx < 3; ++row_idx) {
            int flat_idx = (square_row + row_idx) * 9 + (square_col + col_idx);
            if (flat_idx != idx) {
                mark_false(board, square_row + row_idx, square_col + col_idx, val);
            }
        }
    }
}

int verify(Board* board) {
    for (int idx = 0; idx < 81; ++idx) {
        if (board->counts[idx] == 0) {
            return 0;
        }
    }
    return 1;
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
            if (board->counts[idx] == 1) {
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
            if (board->counts[idx] == 1) {
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
            if (board->counts[idx] == 1) {
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
    for (int idx = 0; idx < 81; ++idx) {
        ret &= board->counts[idx] == 1;
    }
    return ret;
}

void print_board(Board board) {
    const char* digits = "123456789";

    for (int row = 0; row < 9; ++row) {
        for (int col = 0; col < 9; ++col) {
            int idx = row * 9 + col;
            int val = __tzcnt_u32(board.flags[idx]);

            if (board.counts[idx] == 1 && val >= 0 && val < 9) {
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

Solution solve_from_candidates(Stack* stack_ptr) {
    Solution solution = (Solution) {make_empty_board(), 0};

    while(stack_nonempty(stack_ptr)) {
        State state = stack_pop(stack_ptr);
        //printf("Stack popped!\n");


        for(int idx = state.idx; idx < 81; ++idx) {
            //print_flags(&state.current);

            //debug_verify(&state.current);

            if (!verify(&state.current)) {
                //printf("Verify failed!\n");
                break;
            } else if (state.current.counts[idx] == 1) {
                //printf("Nothing to do here!\n");
                if (is_solution(&state.current)) {
                    solution.solution = state.current;
                    solution.is_solved = 1;
                    return solution;
                } else {
                    continue;
                }
            } else {
                //printf("Adding new branch!\n");
                State next = state;
                int col = idx % 9;
                int row = idx / 9;
                int val = __tzcnt_u32(state.current.flags[idx]);

                mark_false(&next.current, row, col, val);
                stack_push(stack_ptr, next);

                mark_true(&state.current, row, col, val);
            }
        }
    }

    return solution;
}

Solution solve_one(const char* problem) {
    State state = (State) {make_empty_board(), 0};
    for (int idx = 0; idx < 81; ++idx) {
        if ((problem[idx] != '0') && (problem[idx] != '.')) {
            int col = idx % 9;
            int row = idx / 9;
            int val = problem[idx] - '1';
            
            mark_true(&state.current, row, col, val);
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
    int64_t step = has_solution
                 ? (81 + 1 /* comma */ + 81 + 1 /* newline */)
                 : (81 + 1);
    while (buffer_size > step) {
        const char* problem_ptr = current;

        current += step;
        buffer_size -= step;

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
}

int main() {
    solve_from_csv("sudoku.csv", 1 /* has_solution */);
    solve_from_csv("top1465.csv", 0 /* has_solution */);

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
