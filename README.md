# choochoos!

An operating system for CS 452 at the University of Waterloo.

Written by [Daniel Prilik](https://prilik.com) and [James Hageman](https://jameshageman.com).

## Building

The default target is updated according the the most-recent assignment, so building is as straightforward as running:

```bash
make
```

The executable will be written to `k1.elf` in the root of the repository.

Additional debug statements can be enabled by invoking `make DEBUG=1` (you might need to `make clean` first).

## Documentation

Documentation is in [k1.pdf](./k1.pdf), and can be built (from markdown sources) via `make k1.pdf`. To build the docs, you must have [pandoc](https://pandoc.org/installing.html) installed.
