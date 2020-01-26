# choochoos!

An operating system for CS 452 at the University of Waterloo.

Written by [Daniel Prilik](https://prilik.com) and [James Hageman](https://jameshageman.com).

## Building

From the repository root, run:

```bash
make release USER_FOLDER=<assignment> # e.g: make release USER_FOLDER=k1
```

The final executable is output at `bin/choochoos.elf`.

_Note_: omiting `release` from the `make` command will enable debug logging and assertions.

## Documentation

Documentation for each assignment can be found under the [docs](./docs/) folder. Each assignment has it's own directory with associated documentation.

e.g: for assignment K1, the documentation is under [docs/k1](./docs/k1/).
