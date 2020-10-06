#include "kernel_functions.h"
list* newList (){
  //empty list block
  list* list = calloc(1,sizeof(list));
  //empty listobject as head and tail
  list->pHead = calloc(1,sizeof(listobj));
  list->pTail = calloc(1,sizeof(listobj));
  //points to head and tail 
  list->pHead->pNext = list->pTail;
  list->pTail->pPrevious = list->pHead;
  list->pHead->pPrevious = list->pHead;
  list->pTail->pNext = list->pTail;
  
  return list;
}

void insertList(list* list, listobj* newObj){
  listobj* temp = list->pHead->pNext;
  while(temp != temp->pNext){
    //insert and sorts after deadline
    if(newObj->pTask->Deadline < temp->pTask->Deadline){
      break;
    }
    else{
      temp = temp->pNext;
    }
  }
  //updates the pointers for list objects
  newObj->pNext = temp;
  newObj->pPrevious = temp->pPrevious;
  temp->pPrevious->pNext = newObj;
  temp->pPrevious = newObj;
}

TCB* firstTCB(list* list){
  return list->pHead->pNext->pTask;
}

listobj* removeTask(list* list){
  listobj* temp = list->pHead->pNext;
  list->pHead->pNext = temp->pNext;
  temp->pNext->pPrevious = temp->pPrevious;
  return temp;
}