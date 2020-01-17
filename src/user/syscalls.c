int Create(int priority, void (*function)()){
    __asm__ volatile ("swi #4");
}

int MyTid(){
    __asm__ volatile ("swi #3");
}

int MyParentTid(){
    __asm__ volatile ("swi #2");
}

int Exit(){
    __asm__ volatile ("swi #1");
}

void Yield(){
    __asm__ volatile ("swi #0");
}