# The `choochoos` Kernel

Written by Daniel Prilik (dprilik - 20604934) and James Hageman (jdhagema - 20604974).

## Project Structure

### Navigating the Repo

`choochoos` follows a fairly standard C/C++ project structure, whereby all header files are placed under the `include` directory, with corresponding implementation files placed under the `src` directory.

At the moment, both the `choochoos` kernel and userspace live under the same `include` and `src` directory, though we plan to separate kernel-specific files and userspace-specific files at some point.

### Building `choochoos`

For details on building the project, see the [../../README.md](`README.md`) at the root of this repository.

## Architectural Overview

Our entry point is not actually `main`, but `_start` defined in `src/boilerplate/crt1.cc`. `_start` does couple key things for the initialization of the kernel:
- The link register is stored in the `redboot_return_addr` variable, so we can exit from any point in the kernel's execution by jumping directly to `redboot_return_addr`
- The BSS is zeroed
- All global variable constructors are run

Then we jump into `main`, which configures the `COM2` UART and jumps to `kmain`.

`kmain` executes our scheduling loop, using a singleton instance of a class called `Kernel`:

```cpp
static kernel::Kernel kern;

int kmain() {
    kprintf("Hello from the choochoos kernel!");

    kern.initialize(FirstUserTask);

    while (true) {
        int next_task = kern.schedule();
        if (next_task < 0) break;
        kern.activate(next_task);
    }

    kprintf("Goodbye from choochoos kernel!");

    return 0;
}
```

The scheduling loop differs from the one described in lecture only in that `Kernel::activate` does not return a syscall decription to handle. Instead, activate returns only after the syscall made by the executing task is handled. Nonetheless, the kernel can be broken down into three main parts: scheduling, context switching, and syscall handling.

### Scheduling

### Context Switching

### Handling syscalls

