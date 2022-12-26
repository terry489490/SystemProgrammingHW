#include <unistd.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ctype.h>

#define ERR_EXIT(a) do { perror(a); exit(1); } while(0)

#define OBJ_NUM 3

#define FOOD_INDEX 0
#define CONCERT_INDEX 1
#define ELECS_INDEX 2
#define RECORD_PATH "./bookingRecord"
#define HEAD 902001
#define MAXFD 1024

static char* obj_names[OBJ_NUM] = {"Food", "Concert", "Electronics"};

typedef struct {
	char hostname[512];  // server's hostname
	unsigned short port;  // port to listen
	int listen_fd;  // fd to wait for a new connection
} server;

typedef struct {
	char host[512];  // client's host
	int conn_fd;  // fd to talk with client
	char buf[512];  // data sent by/to client
	size_t buf_len;  // bytes used by buf
	int id;
	int done;
	int wait_for_write;  // used by handle_read to know if the header is read or not.
} request;

server svr;  // server
request* requestP = NULL;  // point to a list of requests
int maxfd;  // size of open file descriptor table, size of request list

const char* accept_read_header = "ACCEPT_FROM_READ";
const char* accept_write_header = "ACCEPT_FROM_WRITE";
const unsigned char IAC_IP[3] = "\xff\xf4";

static void init_server(unsigned short port);
// initailize a server, exit for error

static void init_request(request* reqP);
// initailize a request instance

static void free_request(request* reqP);
// free resources used by a request instance

typedef struct {
	int id;          // 902001-902020
	int bookingState[OBJ_NUM]; // 1 means booked, 0 means not.
}record;

int myatoi(int *nums, char* buf);

void mylock_function(request *reqP, int file_fd, short type);

short mylock_check(request *reqP, int file_fd, short type);

int self_lock[32];
int handle_read(request* reqP, int file_fd) {
    /*  Return value:
     *      1: read successfully
     *      0: read EOF (client down)
     *     -1: read failed
     */
	int r;
	char buf[512];
	memset(buf, 0, sizeof(buf));

    // Read in request from client	
	r = read(reqP->conn_fd, buf, sizeof(buf));
	if (r < 0) return -1;
	if (r == 0) return 0;
	char* p1 = strstr(buf, "\015\012");
	int newline_len = 2;
	
	int offset, result;

	if (p1 == NULL) {
		p1 = strstr(buf, "\012");
			if (p1 == NULL) {
				if (!strncmp(buf, IAC_IP, 2)) {
                // Client presses ctrl+C, regard as disconnection
					fprintf(stderr, "Client presses ctrl+C....\n");
					if(reqP->id != 0){
						mylock_function(reqP, file_fd, F_UNLCK);
					}
					return 5;
				}
			ERR_EXIT("this really should not happen...");
		}
	}

#ifdef READ_SERVER      		
	if(reqP->done){
		char temp[8];
		strncpy(temp, buf, sizeof(char) * 4);
		temp[4] = '\0';
		if(strcmp(temp, "Exit") == 0){
			strcpy(reqP->buf, temp);
			
			mylock_function(reqP, file_fd, F_UNLCK);
			return 1;
		}
		else return 0;
	}
	else{
		record temp;
		int id = (int)atoi(buf);
		reqP->id = id;
		offset = (id - HEAD);
		if(offset >= 20 || offset < 0)
			return 0;

		result = mylock_check(reqP, file_fd, F_RDLCK);
		
		if(result == F_WRLCK){
			return 4;
		}
		else{
			mylock_function(reqP, file_fd, F_RDLCK);
		}
		
		lseek(file_fd, sizeof(record) * offset, SEEK_SET);
		read(file_fd, &temp, sizeof(record));
		
		sprintf(buf, "%s: %d booked\n%s: %d booked\n%s: %d booked\n\n", obj_names[FOOD_INDEX], temp.bookingState[FOOD_INDEX]
			, obj_names[CONCERT_INDEX], temp.bookingState[CONCERT_INDEX], obj_names[ELECS_INDEX], temp.bookingState[ELECS_INDEX]);
	
		memmove(reqP->buf, buf, sizeof(buf));
		reqP->buf[sizeof(buf) - 1] = '\0';
		reqP->buf_len = sizeof(buf)-1;
		return 1;
	}
#elif defined WRITE_SERVER
	if(reqP->wait_for_write == 1){
		record temp;
		int id = (int)atoi(buf);
		reqP->id = id;
		offset = (id - HEAD);
		if(offset >= 20 || offset < 0)
			return 0;
		
		result = mylock_check(reqP, file_fd, F_WRLCK);
		
		if(result != F_UNLCK || self_lock[reqP->id - HEAD] == 1){
			return 4;
		}
		else{
			mylock_function(reqP, file_fd, F_WRLCK);
		}

		lseek(file_fd, sizeof(record) * offset, SEEK_SET);
		read(file_fd, &temp, sizeof(record));
		
		sprintf(buf, "%s: %d booked\n%s: %d booked\n%s: %d booked\n\n", obj_names[FOOD_INDEX], temp.bookingState[FOOD_INDEX]
			, obj_names[CONCERT_INDEX], temp.bookingState[CONCERT_INDEX], obj_names[ELECS_INDEX], temp.bookingState[ELECS_INDEX]);
	
		memmove(reqP->buf, buf, sizeof(buf));
		reqP->buf[sizeof(buf) - 1] = '\0';
		reqP->buf_len = sizeof(buf)-1;
	}
	else{
		record temp;
		int nums[4], idx = 0;
		char *token;
		
		int gg = myatoi(nums, buf);
		if(gg == 0){
			mylock_function(reqP, file_fd, F_UNLCK);
			return 0;
		}

		offset = (reqP->id - HEAD);
		lseek(file_fd, sizeof(record) * offset, SEEK_SET);
		read(file_fd, &temp, sizeof(record));
		
		int ok = 1;
		int total = 0;
		for(int i = 0; i < 3; i++){
			total += temp.bookingState[i];
			total += nums[i];
			if(total > 15){
				ok = 0;
				break;
			}
		}		
		if(ok == 0)	{
			mylock_function(reqP, file_fd, F_UNLCK);
			return 2;
		}
			
		for(int i = 0; i < 3; i++){
			total = temp.bookingState[i] + nums[i];
			if(total < 0){
				ok = 0;
				break;
			}
		}	
		if(ok == 0)	{
			mylock_function(reqP, file_fd, F_UNLCK);
			return 3;
		}
		
		for(int i = 0; i < 3; i++)
			temp.bookingState[i] += nums[i];
			
		lseek(file_fd, sizeof(record) * offset, SEEK_SET);
		write(file_fd, &temp, sizeof(record));
		
		lseek(file_fd, sizeof(record) * offset, SEEK_SET);
		read(file_fd, &temp, sizeof(record));
		
		mylock_function(reqP, file_fd, F_UNLCK);
		
		sprintf(buf, "%s: %d booked\n%s: %d booked\n%s: %d booked\n", obj_names[FOOD_INDEX], temp.bookingState[FOOD_INDEX]
			, obj_names[CONCERT_INDEX], temp.bookingState[CONCERT_INDEX], obj_names[ELECS_INDEX], temp.bookingState[ELECS_INDEX]);
		memmove(reqP->buf, buf, sizeof(buf));
		reqP->buf[sizeof(buf) - 1] = '\0';
		reqP->buf_len = sizeof(buf)-1;
	}
#endif


	return 1;
}

int main(int argc, char** argv) {
    // Parse args.
	if (argc != 2) {
		fprintf(stderr, "usage: %s [port]\n", argv[0]);
		exit(1);
	}

	struct sockaddr_in cliaddr;  // used by accept()
	int clilen;

	int conn_fd;  // fd for a new connection with client
	int file_fd;  // fd for file that we open for reading

	file_fd = open(RECORD_PATH, O_RDWR);
	memset(self_lock, 0, sizeof(self_lock));

	char buf[512];
	int buf_len;

    // Initialize server
	init_server((unsigned short) atoi(argv[1]));
	printf("%d\n", maxfd);

    // Loop for handling connections
	fprintf(stderr, "\nstarting on %.80s, port %d, fd %d, maxconn %d...\n", svr.hostname, svr.port, svr.listen_fd, maxfd);
	
	struct  pollfd client[MAXFD];
	for(int i = 0; i < MAXFD; i++)
		client[i].fd = -1;
	client[svr.listen_fd].fd = svr.listen_fd;
	client[svr.listen_fd].events = POLLIN;
	
	while (1) {
        // TODO: Add IO multiplexing
        
        
        // Check new connection
    int nready = poll(client, MAXFD, -1);
   	if(client[svr.listen_fd].revents & POLLIN){
				
   		clilen = sizeof(cliaddr);
			conn_fd = accept(svr.listen_fd, (struct sockaddr*)&cliaddr, (socklen_t*)&clilen);

			if (conn_fd < 0) {
				if (errno == EINTR || errno == EAGAIN) continue;  // try again
					if (errno == ENFILE) {
						(void) fprintf(stderr, "out of file descriptor table ... (maxconn %d)\n", maxfd);
						continue;
					}
				ERR_EXIT("accept");
			}
			
#ifdef WRITE_SERVER
			requestP[conn_fd].wait_for_write = 1;
#endif
			client[conn_fd].events = POLLIN;
			client[conn_fd].fd = conn_fd;
			requestP[conn_fd].conn_fd = conn_fd;
			requestP[conn_fd].done = 0;
			strcpy(requestP[conn_fd].host, inet_ntoa(cliaddr.sin_addr));
			fprintf(stderr, "getting a new request... fd %d from %s\n", conn_fd, requestP[conn_fd].host);
			
		  sprintf(buf, "Please enter your id (to check your booking state):\n");
			write(conn_fd, buf, strlen(buf));
		}


		// TODO: handle requests from clients
		for(int i = svr.listen_fd + 1; i < MAXFD; i++){
			if(requestP[i].conn_fd == -1) continue;
			if(client[requestP[i].conn_fd].revents & POLLIN){
				
				int ret = handle_read(&requestP[i], file_fd); // parse data from client to requestP[conn_fd].buf
	
				if(ret != 1){
					if (ret < 0) {
						fprintf(stderr, "bad request from %s\n", requestP[conn_fd].host);
						continue;
					}
					else if(ret == 0){
						sprintf(buf,"[Error] Operation failed. Please try again.\n");
						write(requestP[i].conn_fd, buf, strlen(buf));
					}
					else if(ret == 2){
						sprintf(buf, "[Error] Sorry, but you cannot book more than 15 items in total.\n");
						write(requestP[i].conn_fd, buf, strlen(buf));
					}
					else if(ret == 3){
						sprintf(buf, "[Error] Sorry, but you cannot book less than 0 items.\n");
						write(requestP[i].conn_fd, buf, strlen(buf));
					}
					else if(ret == 4){
						sprintf(buf, "Locked.\n");
						write(requestP[i].conn_fd, buf, strlen(buf));
					}
					else if(ret == 5){
						//ctrl + c
						;
					}
					
					close(requestP[i].conn_fd);
					free_request(&requestP[i]);
				}
			
#ifdef READ_SERVER
				if(strcmp(requestP[i].buf, "Exit") == 0){
						close(requestP[i].conn_fd);
						free_request(&requestP[i]);
				}
			  else{
					sprintf(buf,"%s",requestP[i].buf);
					write(requestP[i].conn_fd, buf, strlen(buf));
					sprintf(buf, "(Type Exit to leave...)\n");
					write(requestP[i].conn_fd, buf, strlen(buf));
					requestP[i].done = 1;
				}
#elif defined WRITE_SERVER
				if(requestP[i].wait_for_write == 1){
					sprintf(buf,"%s",requestP[i].buf);
					write(requestP[i].conn_fd, buf, strlen(buf)); 
					sprintf(buf, "Please input your booking command. (Food, Concert, Electronics. Positive/negative value increases/decreases the booking amount.):\n");
					write(requestP[i].conn_fd, buf, strlen(buf));
					requestP[i].wait_for_write = 0;
				}
				else{
					sprintf(buf, "Bookings for user %d are updated, the new booking state is:\n", requestP[i].id);
					write(requestP[i].conn_fd, buf, strlen(buf));
					sprintf(buf, "%s", requestP[i].buf);
					write(requestP[i].conn_fd, buf, strlen(buf));
					close(requestP[i].conn_fd);
					free_request(&requestP[i]);
				}
#endif				

			}
		}
	}
	
	free(requestP);
  return 0;
}

int myatoi(int *nums, char* buf){
	int len = strlen(buf), niga = 1, idx = 0, sum = 0;
	for(int i = 0; i < len; i++){
		if(isalpha(buf[i]))
			return 0;
		
		if(buf[i] == '-'){
			if(sum != 0) return 0;
			niga = -1;
		}
		else if(isdigit(buf[i])){
			sum *= 10;
			sum += buf[i] - '0';
		}
		else{
			nums[idx++] = sum * niga;
			sum = 0;
			niga = 1;
			if(idx >= 3) return 1;
		}
			
	}
	return 0;
}

void mylock_function(request *reqP, int file_fd, short type){
	struct flock lock;
	
#ifdef WRITE_SERVER
	if(type == F_UNLCK)
		self_lock[reqP->id - HEAD] = 0;
	else
		self_lock[reqP->id - HEAD] = 1;
		
	lock.l_type = type;
	lock.l_whence = SEEK_SET;
	lock.l_start = (reqP->id - HEAD) * sizeof(record);
	lock.l_len = sizeof(record);
	fcntl(file_fd, F_SETLK, &lock);
	
#elif defined READ_SERVER
	lock.l_type = type;
	lock.l_whence = SEEK_SET;
	lock.l_start = (reqP->id - HEAD) * sizeof(record);
	lock.l_len = sizeof(record);
	
	if(type == F_RDLCK){
		self_lock[reqP->id - HEAD]++;
		fcntl(file_fd, F_SETLK, &lock);
	}
	else if(type == F_UNLCK){
		self_lock[reqP->id - HEAD]--;
		
		if(self_lock[reqP->id - HEAD] == 0){
			fcntl(file_fd, F_SETLK, &lock);
		}
	}
	printf("%d\n", self_lock[reqP->id - HEAD]);
#endif
}

short mylock_check(request *reqP, int file_fd, short type){
	struct flock lock;
	lock.l_type = type;
	lock.l_whence = SEEK_SET;
	lock.l_start = (reqP->id - HEAD) * sizeof(record);
	lock.l_len = sizeof(record);
	lock.l_pid = getpid();
	fcntl(file_fd, F_GETLK, &lock);
	return lock.l_type;
}

// ======================================================================================================
// You don't need to know how the following codes are working
#include <fcntl.h>

static void init_request(request* reqP) {
	reqP->conn_fd = -1;
	reqP->buf_len = 0;
	reqP->id = 0;
	reqP->done = 0;
}

static void free_request(request* reqP) {
    /*if (reqP->filename != NULL) {
        free(reqP->filename);
        reqP->filename = NULL;
    }*/
    init_request(reqP);
}

static void init_server(unsigned short port) {
    struct sockaddr_in servaddr;
    int tmp;

    gethostname(svr.hostname, sizeof(svr.hostname));
    svr.port = port;

    svr.listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (svr.listen_fd < 0) ERR_EXIT("socket");

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);
    tmp = 1;
    if (setsockopt(svr.listen_fd, SOL_SOCKET, SO_REUSEADDR, (void*)&tmp, sizeof(tmp)) < 0) {
        ERR_EXIT("setsockopt");
    }
    if (bind(svr.listen_fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        ERR_EXIT("bind");
    }
    if (listen(svr.listen_fd, 1024) < 0) {
        ERR_EXIT("listen");
    }

    // Get file descripter table size and initialize request table
    maxfd = getdtablesize();
    requestP = (request*) malloc(sizeof(request) * maxfd);
    if (requestP == NULL) {
        ERR_EXIT("out of memory allocating all requests");
    }
    for (int i = 0; i < maxfd; i++) {
        init_request(&requestP[i]);
    }
    requestP[svr.listen_fd].conn_fd = svr.listen_fd;
    strcpy(requestP[svr.listen_fd].host, svr.hostname);

    return;
}
