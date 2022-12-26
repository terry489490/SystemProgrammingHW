#include "status.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdbool.h>

Status status[2];
int p1[2][2], p2[2][2], mypid, myppid, pid[2];
char id;
FILE* log_fd;
char parent_table[32] = {'\0','A','A','B','B','C','D','D','E','E','F','F','K','L'};
int battle_num[32] = {2,2,1,2,2,2,0,0,0,0,1,1,0,0};
char child_name[32][2] = {{'B','C'}, {'D','E'}, {'F', 'o'}, {'G','H'}, {'I','J'}, {'K', 'L'}, {'a', 'b'}, {'c', 'd'}, {'e','f'}, {'g','h'}, {'M','k'}, {'N', 'n'}, {'i', 'j'}, {'l','m'}};

void myread(int fd, int which, bool is_parent){
	read(fd, &status[which], sizeof(Status));

	if(is_parent){
		fprintf(log_fd, "%c,%d pipe from %c,%d %d,%d,%c,%d\n", id, mypid, parent_table[id - 'A'], myppid, status[which].real_player_id, status[which].HP, status[which].current_battle_id, status[which].battle_ended_flag);
	}
	else{
		if(which < battle_num[id - 'A']){
			fprintf(log_fd, "%c,%d pipe from %c,%d %d,%d,%c,%d\n", id, mypid, child_name[id - 'A'][which], pid[which], status[which].real_player_id, status[which].HP, status[which].current_battle_id, status[which].battle_ended_flag);
		}
		else{
			fprintf(log_fd, "%c,%d pipe from %d,%d %d,%d,%c,%d\n", id, mypid, (int)child_name[id - 'A'][which] - 'a', pid[which], status[which].real_player_id, status[which].HP, status[which].current_battle_id, status[which].battle_ended_flag);
		}
	}
}


void mywrite(int fd, int which, bool is_parent){
	if(is_parent){
		fprintf(log_fd, "%c,%d pipe to %c,%d %d,%d,%c,%d\n", id, mypid, parent_table[id - 'A'], myppid, status[which].real_player_id, status[which].HP, status[which].current_battle_id, status[which].battle_ended_flag);
	}
	else{
		if(which < battle_num[id - 'A']){
			fprintf(log_fd, "%c,%d pipe to %c,%d %d,%d,%c,%d\n", id, mypid, child_name[id - 'A'][which], pid[which], status[which].real_player_id, status[which].HP, status[which].current_battle_id, status[which].battle_ended_flag);
		}
		else{
			fprintf(log_fd, "%c,%d pipe to %d,%d %d,%d,%c,%d\n", id, mypid, (int)child_name[id - 'A'][which] - 'a', pid[which], status[which].real_player_id, status[which].HP, status[which].current_battle_id, status[which].battle_ended_flag);
		}
	}

	write(fd, &status[which], sizeof(Status));
}

int attr;
void play(){
	int first, second;

	if(status[0].HP < status[1].HP || (status[0].HP == status[1].HP && status[0].real_player_id < status[1].real_player_id)){
		first = 0;
		second = 1;
	}
	else{
		first = 1;
		second = 0;
	}

	status[second].HP -= status[first].ATK * (status[first].attr == attr ? 2 : 1);
	if(status[second].HP <= 0){
		status[first].battle_ended_flag = 1;
		status[second].battle_ended_flag = 1;
		return;
	}

	status[first].HP -= status[second].ATK * (status[second].attr == attr ? 2 : 1);
	if(status[first].HP <= 0){
		status[first].battle_ended_flag = 1;
		status[second].battle_ended_flag = 1;
		return;
	}

}

int main (int argc, char **argv) {
	if(argc != 3) return 0;

	id = argv[1][0];

	myppid = atoi(argv[2]);
	mypid = getpid();
	int attr_table[32] = {0,1,2,2,0,0,0,1,2,1,1,1,0,2};
	attr = attr_table[id - 'A'];

	char buf[128];
	sprintf(buf, "log_battle%c.txt", id);
	log_fd = fopen(buf, "w");

	for(int i = 0; i <= 1; i++){
		pipe(p1[i]);
		pipe(p2[i]);
		pid[i]  = fork();

		if(pid[i] == 0){
			close(p1[i][1]);
			dup2(p1[i][0], STDIN_FILENO);
			close(p1[i][0]);

			close(p2[i][0]);
			dup2(p2[i][1], STDOUT_FILENO);
			close(p2[i][1]);

			char temp1[8], temp2[8];
			sprintf(temp2, "%d", mypid);

			if(i < battle_num[id - 'A']){
				sprintf(temp1, "%c", child_name[id-'A'][i]);
				execl("./battle", "./battle", temp1, temp2);
			}
			else{
				sprintf(temp1, "%d", child_name[id-'A'][i] - 'a');
				execl("./player", "./player", temp1, temp2);
			}
		}
		else{
			close(p1[i][0]);
			close(p2[i][1]);
		}
	}

	for(int i = 0; i <= 1; i++){
		myread(p2[i][0], i, false);
		status[i].current_battle_id = id;
	}

	int win;
	while(1){
		play();
		for(int i = 0; i <= 1; i++)
			mywrite(p1[i][1], i, false);

		if(status[0].battle_ended_flag == 1){
			if(status[0].HP > 0)
				win = 0;
			else
				win = 1;
			break;
		}

		for(int i = 0; i <= 1; i++)
			myread(p2[i][0], i, false);
	}

	wait(0);

	while(1){
		if(id == 'A'){
			wait(0);
			break;
		}
		else{
			myread(p2[win][0], win, false);
			mywrite(STDOUT_FILENO, win, true);
			myread(STDIN_FILENO, win, true);
			mywrite(p1[win][1], win, false);

			if(status[win].HP <= 0 || (status[win].current_battle_id == 'A' && status[win].battle_ended_flag == 1)){
				wait(0);
				break;
			}
		}
	
	}

	if(id == 'A'){
		sprintf(buf, "Champion is P%d\n", status[win].real_player_id);
		write(STDOUT_FILENO, buf, strlen(buf));
	}

	exit(0);

}
