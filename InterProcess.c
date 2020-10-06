#include "kernel_functions_march_2019.h"
#include "doublelinkedlist.h"
#include "globalVariables.h"
#include "system_sam3x.h"
#include <string.h>

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
} 

//removes message from a mailbox
msg* dequeue(mailbox* mBox){
  msg* temp = mBox->pHead->pNext;
  mBox->pHead->pNext = temp->pNext;
  mBox->pHead->pNext->pPrevious = mBox->pHead;
  
  return temp;
}

mailbox* create_mailbox(uint nof_msg, uint size_of_msg){
  //allocate memory for the Mailbox
  mailbox* newMailBox = calloc(1,sizeof(mailbox));
  //empty mailbox as head and tail
  newMailBox->pHead = calloc(1,sizeof(mailbox));
  newMailBox->pTail = calloc(1,sizeof(mailbox));
  //points to head and tail 
  newMailBox->pHead->pNext = newMailBox->pTail;
  newMailBox->pTail->pPrevious = newMailBox->pHead;
  newMailBox->pHead->pNext = newMailBox->pHead;
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
  static int first;
  msg* newMsg;
  first = 1;
  //saves context
  SaveContext();
  if(first == 1){
    //set: "not first excution
    first = 0;  
    msg* temp = mBox->pHead->pNext;
    bool recieveExist = FALSE;
    //goes through the mailbox to see if there is a reciever
    while(temp != mBox->pTail){
      if(temp->Status == RECIEVER){
        recieveExist = TRUE;
        break;
      }
      temp = temp->pNext;
    }
    if(recieveExist){
      //copy sender's data to data area of the recievers Message
      memcpy(temp->pData, Data, strlen(Data)+1);
      /*memcpy(void* dest, const void* src, std::size_t count
      dest - pointer to the memory location to copy to
      src - pointer to the memory location to copy from
      count - number of bytes to copy*/
      //remove recieving task's Message struct from the mailbox
      temp->pPrevious->pNext = temp->pNext;
      temp->pNext->pPrevious = temp->pPrevious;
      //move recieving task to readylist
      extract(temp->pBlock);
      insertList(readyList, temp->pBlock);
    }
    else{
      //checks if mailbox is empty/full
      if(mBox->nMaxMessages==mBox->nMessages){
        msg* temp = dequeue(mBox);
        mBox->nMessages++;
        //check if there is a blocked task , and if there is one remove from 
        //it from waiting list and put it back in readyList
        if(temp->pBlock != NULL){
          listobj* tempObj = extract(temp->pBlock);
          insertList(readyList, tempObj);
        }
        free(temp->pData);
        free(temp);
      }     
      //allocate a message structure
      newMsg = calloc(1,sizeof(msg));
      //set data pointer
      newMsg->pData = Data; 
      //add message to the mailbox
      enqueue(mBox, newMsg);
      mBox->nMessages++;
      //move sending task from readylist to waitinglist
      listobj* obj = removeTask(readyList);
      insertList(waitingList, obj);
    }
    LoadContext();
  }
  else{
    //if deadline is reached
    if(RunningTask->Deadline < Ticks){
      //disable interrupt
      isr_off();
      //remove send message
      extractMsg(newMsg);
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
}

exception recieve_wait(mailbox* mBox, void* Data){
  isr_off();
  static int first;
  msg* newMsg;
  first = 1;
  SaveContext();
  if(first == 1){
    //set not first execution any more 
    first = 0;
    //checks if the mailbox is full
    if(mBox->nMaxMessages==mBox->nMessages){
      msg* temp = dequeue(mBox);
      mBox->nMessages++;
      //check if there is a blocked task , and if there is one remove from 
      //it from waiting list and put it back in readyList
      if(temp->pBlock != NULL){
        listobj* tempObj = extract(temp->pBlock);
        insertList(readyList, tempObj);
      }
      free(temp->pData);
      free(temp);
    }
    msg* temp = mBox->pHead->pNext;
    bool senderExist = FALSE;
    //goes through the mailbox to see if there is a reciever
    while(temp != mBox->pTail){
      if(temp->Status == SENDER){
        senderExist = TRUE;
        break;
      }
      temp = temp->pNext;
    }
    if(senderExist){
      //copy sender's data to data area of the recievers Message
      memcpy(Data, temp->pData, strlen(Data)+1);
      //remove sending tasks Message struct from the Mailbox
      temp->pPrevious->pNext = temp->pNext;
      temp->pNext->pPrevious = temp->pPrevious;
      //if it's a wait type, then move to sending task to ready list
      if(temp->pBlock != NULL){
        extract(temp->pBlock);
        insertList(readyList, temp->pBlock);
      }
      else{
        free(temp->pData);
      }
    }
    else{
      //allocate a message structure
      newMsg = calloc(1,sizeof(msg));
      //add message to the mailbox
      enqueue(mBox, newMsg);
      mBox->nMessages++;
      //move sending task from readylist to waitinglist
      listobj* obj = removeTask(readyList);
      insertList(waitingList, obj);
    }
    LoadContext();
  }
  else{
    //if deadline is reached
    if(RunningTask->Deadline < Ticks){
      isr_off();
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
}

exception send_no_wait(mailbox* mBox, void* Data){
  
}
