#include "kernel_functions.h"
list* ReadyList;
list* WaitingList;
list* TimerList;
int Ticks;
int KernelMode;
TCB* NextTask;
TCB* PreviousTask;