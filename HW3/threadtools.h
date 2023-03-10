#ifndef THREADTOOL
#define THREADTOOL
#include <setjmp.h>
#include <sys/signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define THREAD_MAX 16  // maximum number of threads created
#define BUF_SIZE 512
#define YIELD 1
#define READ 2
#define EXIT 3
#define max(a, b) (a > b ? a : b)

struct tcb {
    int id;  // the thread id
    jmp_buf environment;  // where the scheduler should jump to
    int arg;  // argument to the function
    int fd;  // file descriptor for the thread
    char buf[BUF_SIZE];  // buffer for the thread
    int i, x, y;  // declare the variables you wish to keep between switches
};

extern int timeslice;
extern jmp_buf sched_buf;
extern struct tcb *ready_queue[THREAD_MAX], *waiting_queue[THREAD_MAX];
/*
 * rq_size: size of the ready queue
 * rq_current: current thread in the ready queue
 * wq_size: size of the waiting queue
 */
extern int rq_size, rq_current, wq_size;
/*
* base_mask: blocks both SIGTSTP and SIGALRM
* tstp_mask: blocks only SIGTSTP
* alrm_mask: blocks only SIGALRM
*/
extern sigset_t base_mask, tstp_mask, alrm_mask;
/*
 * Use this to access the running thread.
 */
#define RUNNING (ready_queue[rq_current])

void sighandler(int signo);
void scheduler();

#define thread_create(func, id, arg) {\
	func(id, arg);\
}

#define thread_setup(id, arg) {\
	ready_queue[rq_size] = (struct tcb*)malloc(sizeof(struct tcb)); \
	ready_queue[rq_size]->id = id; \
	ready_queue[rq_size]->arg = arg; \
	ready_queue[rq_size]->x = 0;\
	ready_queue[rq_size]->y = 0;\
	\
	char name[BUF_SIZE]; \
	sprintf(name, "%d_%s", id, __func__); \
	mkfifo(name, 0644); \
	ready_queue[rq_size]->fd = open(name, O_RDONLY | O_NONBLOCK); \
	\
	if(setjmp(ready_queue[rq_size]->environment) == 0){\
		rq_size++; \
		return; \
	} \
}

#define thread_exit() {\
	char name[BUF_SIZE]; \
	close(RUNNING->fd); \
	sprintf(name, "./%d_%s", RUNNING->id, __func__); \
	unlink(name); \
	longjmp(sched_buf, EXIT);\
}

#define thread_yield() {\
	if(setjmp(RUNNING->environment) == 0){ \
		sigset_t waiting_mask; \
		sigpending(&waiting_mask); \
		if(sigismember(&waiting_mask, SIGTSTP))\
			sigsuspend(&alrm_mask); \
		if(sigismember(&waiting_mask, SIGALRM)){\
			sigsuspend(&tstp_mask); \
		}\
	}\
}

#define async_read(count) ({\
	if(setjmp(RUNNING->environment) == 0){ \
		longjmp(sched_buf, READ); \
	} \
	else{ \
		read(RUNNING->fd, RUNNING->buf, count); \
	} \
})

#endif // THREADTOOL
