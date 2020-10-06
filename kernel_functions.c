#include "kernel_functions.h"
#include "globalVariables.h"
#include "doublelinkedlist.h"

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
  ReadyList = newList();
  WaitingList = newList();
  TimerList = newList();
  //set tick counter to zero
  Ticks = 0;
  //create an idle task
  exception status = create_task(*idletask, UINT_MAX);
  if(status == FAIL){
    return FAIL;
  }
  if(ReadyList == NULL || WaitingList == NULL || TimerList == NULL){
    return FAIL;
  }
  //INIT is from kernel_functions_march_2019.h
  KernelMode = INIT;
  return OK;
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
  //sets the SP to point to the stack segment
  taskTCB->SP = &( taskTCB->StackSeg[STACK_SIZE-9] ) ;   
  //insert new task in ready list and returns status
  listobj* obj = create_listobj(taskTCB);
  if(KernelMode == INIT){
    insertList(ReadyList, obj);
    return OK;
  }
  //disable interrupts
  else{
    isr_off();
    PreviousTask = NextTask;
    insertList(ReadyList, obj);
    NextTask =  firstTCB(ReadyList);
    SwitchContext();
  }
  return OK;
}

void run(){
  Ticks = 0;
  KernelMode = RUNNING;
  NextTask = firstTCB(ReadyList);
  LoadContext_In_Run();
}

void terminate(){
  isr_off();
  listobj* leavingObj = removeTask(ReadyList);
  NextTask = firstTCB(ReadyList);
  switch_to_stack_of_next_task();
  free(leavingObj);
  LoadContext_In_Terminate();
}  

//removes listobj from its current list
listobj* extract(listobj* pObj){
  pObj->pPrevious->pNext = pObj->pNext;
  pObj->pNext->pPrevious = pObj->pPrevious;
  
  return pObj;
}

//removes message from its current mailbox
msg* extractMsg(msg* pObj){
  pObj->pPrevious->pNext = pObj->pNext;
  pObj->pNext->pPrevious = pObj->pPrevious;
  
  return pObj;
}

//adds message to a mailbox
void enqueue(mailbox* mBox, msg* Msg){
  mBox->pTail->pPrevious->pNext =  Msg;
  Msg->pPrevious =  mBox->pTail->pPrevious;
  Msg->pNext = mBox->pTail;
  mBox->pTail->pPrevious = Msg;
  mBox->nMessages++;
} 

//removes message from a mailbox
msg* dequeue(mailbox* mBox){
  msg* temp = mBox->pHead->pNext;
  mBox->pHead->pNext = temp->pNext;
  mBox->pHead->pNext->pPrevious = mBox->pHead;
  mBox->nMessages--;
  return temp;
}

mailbox* create_mailbox(uint nof_msg, uint size_of_msg){
  //allocate memory for the Mailbox
  mailbox* newMailBox = (mailbox*)calloc(1,sizeof(mailbox));
  //empty mailbox as head and tail
  newMailBox->pHead = (msg*)calloc(1,sizeof(msg));
  newMailBox->pTail = (msg*)calloc(1,sizeof(msg));
  //points to head and tail 
  newMailBox->pHead->pNext = newMailBox->pTail;
  newMailBox->pTail->pPrevious = newMailBox->pHead;
  newMailBox->pHead->pPrevious = newMailBox->pHead;
  newMailBox->pTail->pNext = newMailBox->pTail;
  
  newMailBox->nDataSize = size_of_msg;
  newMailBox->nMaxMessages = nof_msg;
  
  return newMailBox;
}

exception remove_mailbox(mailbox* mBox){
  //free the memory for the Mailbox
  if(mBox->nMessages == 0){
    free(mBox->pHead);
    free(mBox->pTail);
    free(mBox);
    
    return OK;
  }
  else{
    return NOT_EMPTY;
  }
}

exception send_wait(mailbox* mBox, void* Data){
  //disable interrupt
  isr_off();
  bool receiveExist = FALSE;
  msg* temp = mBox->pHead->pNext;
  msg* newMsg;
  
  //goes through the mailbox to see if there is a reciever
  while(temp != temp->pNext){
    if(temp->Status == RECEIVER){
      receiveExist = TRUE;
      break;
    }
    temp = temp->pNext;
  }
  if(receiveExist){
    //copy sender's data to data area of the recievers Message
    memcpy((temp->pData), Data, mBox->nDataSize);
    /*memcpy(void* dest, const void* src, std::size_t count
    dest - pointer to the memory location to copy to
    src - pointer to the memory location to copy from
    count - number of bytes to copy*/
    //remove recieving task's Message struct from the mailbox
    temp->pPrevious->pNext = temp->pNext;
    temp->pNext->pPrevious = temp->pPrevious;
    
    PreviousTask = NextTask;
    extract(temp->pBlock);
    insertList(ReadyList, temp->pBlock);
    NextTask =  firstTCB(ReadyList);
  }
  else{
    //checks if mailbox is full
    if(mBox->nMaxMessages == mBox->nMessages){
      msg* temp = dequeue(mBox);
      //check if there is a blocked task , and if there is one remove from 
      //it from waiting list and put it back in readyList
      if(temp->pBlock != NULL){
        listobj* tempObj = extract(temp->pBlock);
        insertList(ReadyList, tempObj);
      }
      free(temp->pData);
      free(temp);
    }     
    //allocate a message structure
    newMsg = (msg*)calloc(1,sizeof(msg));
    newMsg->pData = (char*)calloc(1, mBox->nDataSize);
    newMsg->Status = SENDER;
    //set data pointer
    memcpy((newMsg->pData), Data, mBox->nDataSize);
    //add message to the mailbox
    enqueue(mBox, newMsg);
    PreviousTask = NextTask;
    //move sending task from readylist to waitinglist
    listobj* obj = removeTask(ReadyList);
    newMsg->pBlock = obj;
    insertList(WaitingList, obj);
    NextTask =  firstTCB(ReadyList);
  }
  SwitchContext();
  
  //if deadline is reached
  if(NextTask->Deadline < Ticks){
    //disable interrupt
    isr_off();
    //remove send message
    extractMsg(newMsg);
    mBox->nMessages--;
    //frees the mBox
    free(mBox->pHead);
    free(mBox->pTail);
    free(mBox);
    //interrupt on
    isr_on();
    
    return DEADLINE_REACHED;
  }
  else{
    return OK;
  }
}

exception receive_wait(mailbox* mBox, void* Data){
  isr_off();
  msg* newMsg;
  
  //checks if the mailbox is full
  if(mBox->nMaxMessages==mBox->nMessages){
    msg* temp = dequeue(mBox);
    //check if there is a blocked task , and if there is one remove from 
    //it from waiting list and put it back in readyList
    if(temp->pBlock != NULL){
      listobj* tempObj = extract(temp->pBlock);
      insertList(ReadyList, tempObj);
    }
    free(temp->pData);
    free(temp);
  }
  msg* temp = mBox->pHead->pNext;
  bool senderExist = FALSE;
  //goes through the mailbox to see if there is a sender
  while(temp != temp->pNext){
    if(temp->Status == SENDER){
      senderExist = TRUE;
      break;
    }
    temp = temp->pNext;
  }
  if(senderExist){
    //copy sender's data to data area of the recievers Message
    memcpy(Data, (temp->pData), mBox->nDataSize);
    //remove sending tasks Message struct from the Mailbox
    extractMsg(temp);
    mBox->nMessages--;
    //if it's a wait type, then move to sending task to ready list
    if(temp->pBlock != NULL){
      //removes it from WaitingList
      extract(temp->pBlock);
      PreviousTask = NextTask;
      insertList(ReadyList, temp->pBlock);
      NextTask = firstTCB(ReadyList);
    }
    else{
      free(temp->pData);
    }
  }
  else{
    //allocate a message structure
    newMsg = (msg*)calloc(1,sizeof(msg));
    newMsg->Status = RECEIVER;
    //add message to the mailbox
    enqueue(mBox, newMsg);
    PreviousTask = NextTask;
    //move sending task from readylist to waitinglist
    listobj* obj = removeTask(ReadyList);
    newMsg->pBlock = obj;
    insertList(WaitingList, obj);
    NextTask = firstTCB(ReadyList);
  }
  SwitchContext();
  
  //if deadline is reached
  if(NextTask->Deadline < Ticks){
    isr_off();
    //remove receive message
    extractMsg(newMsg);
    mBox->nMessages--;
    free(newMsg->pData);
    free(newMsg);
    isr_on();
    
    return DEADLINE_REACHED;
  }
  else{
    return OK;
  }
}

exception send_no_wait(mailbox* mBox, void* Data){
  
  isr_off();
  msg* temp = mBox -> pHead->pNext;
  bool receiverExist = FALSE;
  //goes through the mailbox to see if there is a receiver
  while(temp != temp->pNext){
  if(temp->Status == RECEIVER){
  receiverExist = TRUE;
  break;
}
  temp = temp->pNext;
}
  if(receiverExist){
  memcpy(temp->pData,Data,sizeof(Data));
  //remove recieving task's Message struct from the mailbox
  temp->pPrevious->pNext = temp->pNext;
  temp->pNext->pPrevious = temp->pPrevious;
  PreviousTask = NextTask;
  //move reciving task to ReadyList
  insertList(ReadyList, temp->pBlock);
  NextTask = firstTCB(ReadyList); 
  SwitchContext();
}
  else{
  //allocate a message structure
  msg* newMsg = (msg*)calloc(1,sizeof(msg));
  newMsg->pData = (char*)calloc(1, mBox->nDataSize);
  newMsg->Status = SENDER;
  //copy data to the message
  memcpy(newMsg->pData, Data, sizeof(Data));
  //if the mailbox is full then remove the oldest message struct
  if(mBox->nMaxMessages==mBox->nMessages){
  msg* temp = dequeue(mBox);
}
  //add message to the mailbox
  enqueue(mBox, newMsg);
  isr_on();
}
  return OK;
  
  
  
  /*isr_off();
  if(mBox->pHead->pNext->Status < 0){
    msg* temp = dequeue(mBox);
    memcpy(temp->pData, Data, sizeof(Data));
    PreviousTask = NextTask;
    listobj* obj = ftemp->pBlock);
    insertList(ReadyList, temp->pBlock);
    NextTask = ReadyList->pHead->pNext->pTask;
    SwitchContext();
  }
  else{
    msg* newMsg = (msg*)calloc(1, sizeof(msg));
    newMsg->pData = (char*)calloc(1, mBox->nDataSize);
    newMsg->Status = SENDER;
    memcpy(newMsg->pData, Data, sizeof(Data));
    if(mBox->nMaxMessages == mBox->nMessages){
      msg* temp = dequeue(mBox);
      free(temp->pData);
      free(temp);
    }
    
    enqueue(mBox, newMsg);
    isr_on();
    
    
  }
  return OK;
  */
}

exception receive_no_wait(mailbox* mBox, void* Data){
  /*isr_off();
  msg* temp = mBox -> pHead->pNext;
  bool senderExist = FALSE;
  //goes through the mailbox to see if there is a receiver
  while(temp != temp->pNext){
  if(temp->Status == SENDER){
  senderExist = TRUE;
  break;
}
  temp = temp -> pNext;
}
  if(senderExist){
  //copy sender's data to receiving task's data area
  memcpy(Data, (temp->pData), sizeof(Data));
  printf("data: %c", Data);
  //remove sending tasks Message struct from the Mailbox
  temp->pPrevious->pNext = temp->pNext;
  temp->pNext->pPrevious = temp->pPrevious;
  //if message was of wait type then update PreviousTask and NextTask
  if(temp->pBlock != NULL){
  PreviousTask = NextTask;
  insertList(ReadyList, temp->pBlock);
  NextTask = firstTCB(ReadyList);
  SwitchContext();
}
    else{
  //free sender's data area
  free(temp->pData);
  free(temp);
}
}
  else{
  return FAIL;
}
  isr_on();
  return OK;
  */
  
  isr_off();
  if(mBox->pHead->pNext->Status > 0){
    msg* newMsg = dequeue(mBox);
    memcpy(Data, newMsg->pData, sizeof(Data));
    if(newMsg->pBlock != NULL){
      PreviousTask = NextTask;
      listobj* temp = extract(newMsg->pBlock);
      insertList(ReadyList, temp);
      NextTask = temp->pTask;
      SwitchContext();
    }
    else{
      free(newMsg->pData);
      free(newMsg);
      isr_on();
    }
  }
  else{
    isr_on();
    return FAIL;
  }
  return OK;
}

//TIMING FUNCTIONS
exception wait(uint nTicks){
  isr_off();
  PreviousTask = NextTask;
  listobj* obj = removeTask(ReadyList);
  obj->nTCnt = nTicks;
  insertList(TimerList, obj);
  NextTask = firstTCB(ReadyList);
  SwitchContext();
  if(NextTask->Deadline < Ticks){
    return DEADLINE_REACHED;
  }
  else{
    return OK;
  }
}

void set_ticks( uint nTicks){
  Ticks = nTicks;
}

uint ticks(void){
  return Ticks;
}

uint deadline(void){
  return NextTask->Deadline;
}

void set_deadline(uint deadline){
  isr_off();
  //set the deadline field in the calling TCB
  NextTask->Deadline = deadline;
  PreviousTask = NextTask;
  listobj* temp = removeTask(ReadyList);
  //reschedule ReadyList
  insertList(ReadyList, temp);
  NextTask = firstTCB(ReadyList);
  SwitchContext();
}

void TimerInt(void){
  Ticks++;
  listobj* temp = TimerList->pHead->pNext;
  //goes through the list
  while(temp != temp->pNext){
    temp->nTCnt--;
    //check the TimerList for tasks that are ready for execution 
    //or if its deadline has expired
    if(temp->nTCnt == 0 || temp->pTask->Deadline < Ticks){
      listobj * temp2 = extract(temp);
      temp = temp->pNext;
      //move these to ReadyList
      insertList(ReadyList, temp2);
    }
    else{
      temp = temp->pNext;
    }
  }
  temp = WaitingList->pHead->pNext;
  //goes through the list
  while(temp != temp->pNext){
    //check the waitinglist for tasks that have expired deadlines
    if( temp->pTask->Deadline < Ticks){
      listobj * temp2 = extract(temp);
      temp = temp->pNext;
      //move these to readylist and clean up their Mailbox entry
      insertList(ReadyList, temp2);
      extractMsg(temp2->pMessage);
      free(temp2->pMessage);
      PreviousTask = NextTask;
      NextTask = firstTCB(ReadyList);
    }
    else{
      temp = temp->pNext;
    }
  }
}
