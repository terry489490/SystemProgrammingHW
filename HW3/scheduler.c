#include "threadtools.h"
#include <poll.h>

/*
 * Print out the signal you received.
 * If SIGALRM is received, reset the alarm here.
 * This function should not return. Instead, call longjmp(sched_buf, 1).
 */
void sighandler(int signo) {
	if(signo == SIGALRM){
		printf("caught SIGALRM\n");
		alarm(timeslice);
	}
	else if(signo == SIGTSTP){
		printf("caught SIGTSTP\n");
	}
	sigprocmask(SIG_SETMASK, &base_mask, NULL);
	longjmp(sched_buf, YIELD);
}

/*
 * Prior to calling this function, both SIGTSTP and SIGALRM should be blocked.
 */
void scheduler() {
   int type = setjmp(sched_buf);

   if(type == 0) longjmp(RUNNING->environment, 4);
   struct pollfd checker;
   checker.events = POLLIN;

   int new_wq_size = 0;
   for(int i = 0; i < wq_size; i++){
	   checker.fd = waiting_queue[i]->fd;
	   poll(&checker, 1, 0);

	   if(checker.revents & POLLIN){
		   ready_queue[rq_size++] = waiting_queue[i];
	   }
	   else{
		   waiting_queue[new_wq_size++] = waiting_queue[i];
	   }
   }
   wq_size = new_wq_size;
   

   if(type == YIELD){
	   rq_current++;
//	   rq_current %= rq_size;
   }
   else if(type == READ){
	   waiting_queue[wq_size++] = ready_queue[rq_current];
	   ready_queue[rq_current] = ready_queue[rq_size-1];
	   rq_size--;
   }
   else if(type == EXIT){
	   free(ready_queue[rq_current]);
	   ready_queue[rq_current] = ready_queue[rq_size - 1];
	   rq_size--;
   }
 

   if(rq_size == 0 && wq_size == 0) return; 
   else if(rq_size == 0){
	   while(rq_size == 0){
	  	 struct pollfd checker;
	  	 checker.events = POLLIN;

		 int new_wq_size = 0;
	  	 for(int i = 0; i < wq_size; i++){
		   checker.fd = waiting_queue[i]->fd;
		   poll(&checker, 1, 0);

		   if(checker.revents & POLLIN){
			   ready_queue[rq_size++] = waiting_queue[i];
		   }
		   else{
			   waiting_queue[new_wq_size++] = waiting_queue[i];
		   }
		 }
	  	 wq_size = new_wq_size;
	   }
   }
   if(rq_current == rq_size)
	   rq_current = 0;

   longjmp(RUNNING->environment, 4);
}
