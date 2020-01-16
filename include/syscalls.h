#pragma once

int Create(int priority, void (*function)());
int MyTid();
int MyParentTid();
int Exit();
void Yield();
