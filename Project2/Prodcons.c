#include <stdio.h>
#include <sys/types.h>
#include <linux/unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <sys/mman.h>

#define TRUE 1
#define FALSE 0

struct cs1550_sem{
  int val;
  struct Node *s;
  struct Node *e;
};

void down(struct cs1550_sem * s){
  //Calls down syscall

  syscall(__NR_cs1550_down, s);
}
void up(struct cs1550_sem *s){
  //Calls up syscall
  syscall(__NR_cs1550_up, s);
}

int main(int argc, char *argv[]){
  if(argc!= 4){
    printf("Usage: number of producers, num of consumers, buffer size\n");
    return -1;
  }
  printf("Test\n");
  int prods = atoi(argv[1]);
  int cons = atoi(argv[2]);
  int bufferSize = atoi(argv[3]);
  // Need to allocate memory for 3 semaphores, full, empty and mutex
  void* semaphores = mmap(NULL, sizeof(struct cs1550_sem) * 3, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);

  struct cs1550_sem * full = (struct cs1550_sem* )semaphores;
  struct cs1550_sem * empty = (struct cs1550_sem* )semaphores+1;
  struct cs1550_sem * mutex = (struct cs1550_sem* )semaphores+2;

  //Allocate space for buffer, plus 2 for next producing and consuming locations
  void* buffer = mmap(NULL, sizeof(int)*(bufferSize + 2), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);

  int* producerPointer = (int*) buffer;
  int* consumerPointer = (int*) buffer + 1;
  int* bufferPointer = (int*) buffer + 2;

  //Produce and consume out of the start of the buffer
  *producerPointer = 0;
  *consumerPointer = 0;

  //Set semaphore values
  full->val = 0;
  full->s = NULL;
  full->e = NULL;
  mutex->val = 1;
  mutex->s = NULL;
  mutex->e = NULL;
  empty->val = bufferSize;
  empty->s = NULL;
  empty->e = NULL;
  printf("Where do we die?\n");
  int i = 0;
  for(; i<prods; i++){
    if(fork() == 0){
      printf("Hello from the producer\n");
      int produced;
      while (TRUE) {
        printf("Where do we die?2\n");
        down(full);
        down(mutex);
        printf("Locked\n");
        //Put item into buffer, increment pointer, and print
        produced = *producerPointer;

        bufferPointer[*producerPointer] = produced;
        printf("Producer %c Produced: %d\n", i+65, produced);
        *producerPointer++;
        *producerPointer %= bufferSize; //Incase we go over the size of the buffer
        //Unlock mutex
        up(mutex);
        up(full);

      }
    }
  }

  //Consume Now
  i=0;
  for(; i<cons; i++){
    if(fork()==0){
      printf("Hello from the consumer\n");
      int consumed;
      while (TRUE) {
        down(full);
        down(mutex);
        consumed = bufferPointer[*consumerPointer];
        printf("Consumer %c Consumed %d\n", i+65, consumed);
        *consumerPointer++;
        *consumerPointer %= bufferSize;
        up(mutex);
        up(empty);
      }
    }
  }
  return 0;
}
