#include "kernel_functions_march_2019.h"
#include "doublelinkedlist.h"
#include "system_sam3x.h"
#include "globalVariables.h"
#include <limits.h>
#define SYSTICK_TICK 1500


listobj* create_listobj(TCB* task){
  //creates a new list object
  listobj* newObj = calloc(1,sizeof(listobj));
  //insert and then sort
  newObj->pTask = task;
  return newObj;
}

void idletask(){
  //does nothing
  while(1){
  }
}

exception init_kernel(){
  readyList = newList();
  waitingList = newList();
  timerList = newList();
  Ticks = 0;
  create_task(*idletask, UINT_MAX);
  //INIT is from kernel_functions.h
  KernelMode = INIT;
  return 1;
}



exception create_task(void(*task_body)(), uint deadline){
  //allocate memory for TCB
  TCB* taskTCB = calloc(1,sizeof(TCB));
  //sets deadline in TCB
  taskTCB->Deadline = deadline;
  //sets the TCB's PC to point to the task body
  taskTCB->PC = *task_body;
  taskTCB->SPSR = 0x21000000;
  taskTCB->StackSeg[STACK_SIZE-2] = 0x21000000;
  taskTCB->StackSeg[STACK_SIZE-3] = (unsigned int)task_body;
  taskTCB->StackSeg[STACK_SIZE-4] = 0; 
  //sets the SP to point to the stack segment
  taskTCB->SP = &( taskTCB->StackSeg[STACK_SIZE-9] ) ;   
  //insert new task in ready list and returns status
  listobj* obj = create_listobj(taskTCB);
  if(KernelMode == INIT){
    insertList(readyList, obj);
    return OK;
  }
  //disable interrupts and save context
  else{
    isr_off();
    static int first;
    first = 1;
    //SaveContext();
    //if(first == 1){
      //first = 0;
    //updates previous task
      
      insertList(readyList, obj);
      LoadContext();
    
  }
  return OK;
}

void run(){
  SysTick_Config(SYSTICK_TICK);
  KernelMode = RUNNING;
  isr_on();
  RunningTask = firstTCB(readyList);
  LoadContext();
}

void terminate(){
  listobj* obj = removeTask(readyList);
  RunningTask = firstTCB(readyList);
  LoadContext();
}