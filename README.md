# Sudoku solver

Sudoku solver implemented using AVX2 intrinsics. Provides a Python utility to
benchmark each version of the code and track performance over time.

To run the historical benchmark, first find the first `git` commit to run the
benchmarks from:

```
$ git log --oneline
...
2a417bc AVX2 implementation of marking
...
```

Then run the `run_historical.py` script:

```
$ python ./run_historical.py --first-commit 2a417bc
```
