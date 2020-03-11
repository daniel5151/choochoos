# choochoos!

An operating system for CS 452 at the University of Waterloo.

Written by [Daniel Prilik](https://prilik.com) and [James Hageman](https://jameshageman.com).

## Building

The default target is updated according the the most-recent assignment, so building is as straightforward as running:

```bash
make
```

The executable will be written to an elf file in the root of the repository, who's name matches the current assignment.

Additional debug statements can be enabled by invoking `make DEBUG=1` (though you might need to `make clean` first).

## Documentation

Documentation for each assignment can be found under the `docs/` folder, and can be built (from markdown sources) via `make docs`. _Note:_ building the docs requires [pandoc](https://pandoc.org/installing.html) installed.

Final PDFs for past assignments can be found under the `docs/rendered` folder.

The TC2 docs are available in [t2.pdf](./t2.pdf).
