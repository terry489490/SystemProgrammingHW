#include "threadtools.h"

void fibonacci(int id, int arg) {
    thread_setup(id, arg);

    for (RUNNING->i = 1; ; RUNNING->i++) {
        if (RUNNING->i <= 2)
            RUNNING->x = RUNNING->y = 1;
        else {
            /* We don't need to save tmp, so it's safe to declare it here. */
            int tmp = RUNNING->y;  
            RUNNING->y = RUNNING->x + RUNNING->y;
            RUNNING->x = tmp;
        }
        printf("%d %d\n", RUNNING->id, RUNNING->y);
        sleep(1);

        if (RUNNING->i == RUNNING->arg) {
            thread_exit();
        }
        else {
            thread_yield();
        }
    }
}

void collatz(int id, int arg) {
	thread_setup(id, arg);

	while(RUNNING->arg != 1){
		if(RUNNING->arg % 2 == 0){
			RUNNING->arg /= 2;
		}
		else{
			RUNNING->arg *= 3;
			RUNNING->arg += 1;
		}
		printf("%d %d\n", RUNNING->id, RUNNING->arg);
		sleep(1);

		if(RUNNING->arg == 1){
			thread_exit();
		}
		else{
			thread_yield();
		}
	}
}

void max_subarray(int id, int arg) {
    thread_setup(id, arg);

	// one number at a time???????????????????
	for(RUNNING->i = 1; ; RUNNING->i++){
		async_read(5);

		RUNNING->buf[4] = '\0';
		int val = atoi(RUNNING->buf);
		RUNNING->x = max(val, RUNNING->x + val);
		RUNNING->y = max(RUNNING->y, RUNNING->x);
		
		printf("%d %d\n", RUNNING->id, RUNNING->y);
		sleep(1);

		if(RUNNING->i == RUNNING->arg){
			thread_exit();
		}
		else{
			thread_yield();
		}
	}
}
