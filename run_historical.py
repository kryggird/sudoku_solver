#!/usr/bin/env python
import argparse
from contextlib import contextmanager
from functools import partial
from itertools import dropwhile
import os
from subprocess import run, PIPE
import sys
import tempfile
from time import clock_gettime_ns, CLOCK_MONOTONIC

REPO_PATH = os.path.dirname(__file__)
REPO_NAME = os.path.basename(REPO_PATH)
DATA_FILES = list(
    map(partial(os.path.join, REPO_PATH), 
        #["data/puzzles0_kaggle", "data/puzzles3_magictour_top1465", "data/puzzles5_forum_hardest_1905_11+"]
        ['data/puzzles6_forum_hardest_1106']
    )
)

GIT_LOG_FLAG_SHOW_ONLY_COMMIT_HASH = "--pretty=format:%h"

@contextmanager
def time_it(prefix=""):
    start = clock_gettime_ns(CLOCK_MONOTONIC)
    yield
    delta = clock_gettime_ns(CLOCK_MONOTONIC) - start
    print(f"{prefix} {(delta / 1000 / 1000 / 1000):.3f}s".strip())

def get_commits(first_commit=None):
    command = ["git", "log", GIT_LOG_FLAG_SHOW_ONLY_COMMIT_HASH]
    raw = run(command, stdout=PIPE).stdout
    decoded = reversed([b.decode('utf-8') for b in raw.split()])

    if first_commit is not None:
        def _cond(c):
            return not c.startswith(first_commit)
        output = list(dropwhile(_cond, decoded))
    else:
        output = decoded

    return output

def get_compile_flags(*, debug_verify=True, opt_level=0, perf_output_dir=None):
    if os.path.exists("compile_flags.txt"): 
        with open("compile_flags.txt") as f:
            data = list(map(lambda s: s.strip(), f.readlines()))
        if opt_level > 0:
            data = [f"-O{opt_level}", *data]
        if perf_output_dir is not None:
            data = ['-g', *data]
        if debug_verify:
            data = ["-DDEBUG_VERIFY=1", *data]
        return data
    else:
        return None

def compile(input_file, output_file, *, flags=None):
    command = ["gcc", *flags, input_file, "-o", output_file]
    print(" ".join(command))
    run(command)

def clone_to_tmpdir(first_commit, **flag_kwargs):
    perf_dir = flag_kwargs.pop('perf_output_dir')
    perf_dir = None if perf_dir is None else os.path.abspath(perf_dir)

    with tempfile.TemporaryDirectory() as checkout_td:
        os.chdir(checkout_td)
        run(["git", "clone", "--quiet", REPO_PATH])

        os.chdir(os.path.join(checkout_td, REPO_NAME))

        for commit in get_commits(first_commit):
            run(["git", "checkout", "--quiet", commit])
            flags = get_compile_flags(**flag_kwargs)

            with tempfile.TemporaryDirectory(suffix="-" + commit) as output_td:
                solver_exec = os.path.join(output_td, "solver")
                bitset_exec = os.path.join(output_td, "test_bitset")

                compile("solver.c", solver_exec, flags=flags)
                solver_with_args = [solver_exec, *DATA_FILES]
                with time_it(commit):
                    run(solver_with_args, check=True)

                if os.path.exists("test_bitset.c"):
                    compile("test_bitset.c", bitset_exec, flags=flags)
                    run([bitset_exec], check=True)

                if perf_dir is not None and os.path.exists(perf_dir):
                    output_flag = f'--output={perf_dir}/{commit}.data'
                    run(['perf', 'record', '-e', 'cycles:ppp', output_flag, '--', *solver_with_args])

def make_argument_parser():
    parser = argparse.ArgumentParser()
    parser.add_argument('--first-commit', required=True)
    parser.add_argument('--debug-verify', action='store_true')
    parser.add_argument('--opt-level', type=int, default=0)
    parser.add_argument('--perf-output-dir', default=None)
    return parser

if __name__ == "__main__":
    args = vars(make_argument_parser().parse_args())
    clone_to_tmpdir(**args)
