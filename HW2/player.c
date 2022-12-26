#include "status.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdbool.h>

Status mystatus;
int origin_HP;
int id, myppid, agent_id, log_fd;

void read_status(int id){
	FILE* file_fd = fopen("player_status.txt", "r+");
	
	mystatus.real_player_id = id;
	char buf[128], temp;
	
	rewind(file_fd);

	for(int i = 0; i <= id; i++){
		fscanf(file_fd, "%d %d %s %c %d", &mystatus.HP, &mystatus.ATK, buf, &mystatus.current_battle_id, &mystatus.battle_ended_flag);
		if(strcmp(buf, "FIRE") == 0)
			mystatus.attr = FIRE;
		if(strcmp(buf, "GRASS") == 0)
			mystatus.attr = GRASS;
		if(strcmp(buf, "WATER") == 0)
			mystatus.attr = WATER;
	}
}

int which_agent(){
	switch(mystatus.current_battle_id){
		case 'G':	return 8;
		case 'I':	return 9;
		case 'D':	return 10;
		case 'H':	return 11;
		case 'J':	return 12;
		case 'E':	return 13;
		case 'B':	return 14;
	}
}

char target;
char which_target(){
	switch(id){
		case 0: case 1:	return 'G';
		case 2: case 3: return 'H';
		case 4:	case 5: return 'I';
		case 6:	case 7: return 'J';
		case 8:	case 9:	return 'M';
		case 10: return 'K';
		case 11: case 12: return 'N';
		case 13: return 'L';
		case 14: return 'C';
	}
}

void myread(int fd, bool isfifo){
	read(fd, &mystatus, sizeof(Status));

	char buf[128];
	if(!isfifo){
		sprintf(buf, "%d,%d pipe from %c,%d %d,%d,%c,%d\n", id, getpid(), target, myppid, mystatus.real_player_id, mystatus.HP, mystatus.current_battle_id, mystatus.battle_ended_flag);
		write(log_fd, buf, strlen(buf));
	}
	else{
		sprintf(buf, "log_player%d.txt", mystatus.real_player_id);
		log_fd = open(buf, O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

		sprintf(buf,"%d,%d fifo from %d %d,%d\n", id, getpid(), mystatus.real_player_id, mystatus.real_player_id, mystatus.HP);
		write(log_fd, buf, strlen(buf));
	}
}

void mywrite(int fd, bool isfifo){
	char buf[128];
	if(!isfifo){
		sprintf(buf, "%d,%d pipe to %c,%d %d,%d,%c,%d\n", id, getpid(), target, myppid, mystatus.real_player_id, mystatus.HP, mystatus.current_battle_id, mystatus.battle_ended_flag);
		write(log_fd, buf, strlen(buf));
	}
	else{
		sprintf(buf, "%d,%d fifo to %d %d,%d\n", id, getpid(), agent_id, mystatus.real_player_id, mystatus.HP);
		write(log_fd, buf, strlen(buf));
	}

	write(fd, &mystatus, sizeof(Status));
}

int main (int argc, char **argv) {
	if(argc != 3) return 0;

	char buf[128];

	id = atoi(argv[1]);
	myppid = atoi(argv[2]);
	target = which_target();

	if(0 <= id && id <= 7){
		sprintf(buf, "log_player%d.txt", id);
		log_fd = open(buf, O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

		read_status(id);
		mywrite(STDOUT_FILENO, false);
	}
	else{
		sprintf(buf, "./player%d.fifo", id);
		mkfifo(buf, 0644);
		int fifo_infd = open(buf, O_RDONLY);
		myread(fifo_infd, true);
		mywrite(STDOUT_FILENO, false);
	}

	origin_HP = mystatus.HP;

	while(1){
		while(1){
			myread(STDIN_FILENO, false);
			if(mystatus.battle_ended_flag == 1)
				break;
			mywrite(STDOUT_FILENO, false);
		}
		
		if(mystatus.HP <= 0)
			break;
		if(mystatus.current_battle_id == 'A')
			exit(0);

		mystatus.battle_ended_flag = 0;
		mystatus.HP = (origin_HP - mystatus.HP) / 2 + mystatus.HP;
		mywrite(STDOUT_FILENO, false);
	}

	if(mystatus.current_battle_id != 'A' && id <= 7){
		agent_id = which_agent(mystatus.current_battle_id);
		sprintf(buf, "./player%d.fifo", agent_id);
		mystatus.HP = origin_HP;
		mystatus.battle_ended_flag = 0;

		int fifo_outfd = -1;
		while(fifo_outfd <= 0)
			fifo_outfd = open(buf, O_WRONLY);
		
		mywrite(fifo_outfd, true);
		close(fifo_outfd);
	}

	exit(0);
}
