void FirstUserTask();

namespace foo {
    extern "C" {
        void _putchar(char c) { (void)c; while(1) __asm__ volatile(""); }

        #define STUB(fn) \
            void fn() { while(1) __asm__ volatile(""); }

        STUB(_exit)
        STUB(_sbrk)
        STUB(_kill)
        STUB(_getpid)
        STUB(_write)
        STUB(_close)
        STUB(_fstat)
        STUB(_isatty)
        STUB(_lseek)
        STUB(_read)
        STUB(__aeabi_d2f)

        void FirstUserTask() {
            ::FirstUserTask();
        }
    }
}

