#include "header.h"
#include <sys/mman.h>
#define REAL_LIMIT 5
#define SIZE_LIMIT 1<<10
#define MAX_LEVEL 4
#define REQ_THREAD 32

#ifdef THREAD
typedef struct Range{
	int l, r, level, reqid, id;
}Range;

typedef struct Data{
	char *movie_name[MAX_MOVIES];
	double score[MAX_MOVIES];
}Data;

Data *data;
#elif defined PROCESS
typedef struct Range{
	int l, r, level, reqid, id;
	char *movie_name[MAX_MOVIES];
	double score[MAX_MOVIES];
}Range;
#endif

typedef struct MYREQ{
	int which;
}MYREQ;

movie_profile* movies[MAX_MOVIES];
unsigned int num_of_movies = 0;
unsigned int num_of_reqs = 0;

// Global request queue and pointer to front of queue
// TODO: critical section to protect the global resources
request* reqs[MAX_REQ];
int front = -1;

/* Note that the maximum number of processes per workstation user is 512. * 
 * We recommend that using about <256 threads is enough in this homework. */
pthread_t tid[MAX_CPU][MAX_THREAD]; //tids for multithread

#ifdef PROCESS
pid_t pid[MAX_CPU][MAX_THREAD]; //pids for multiprocess
#endif

//mutex
pthread_mutex_t lock; 

void initialize(FILE* fp);
request* read_request();
int pop();

int pop(){
	front+=1;
	return front;
}

void* mylloc(size_t size) {  
	int protection = PROT_READ | PROT_WRITE;  
	int visibility = MAP_SHARED | MAP_ANONYMOUS;  
	return mmap(NULL, size, protection, visibility, -1, 0);
}

double cross(double* p1, double* p2){
	double sum = 0;
	for(int i = 0; i < NUM_OF_GENRE; i++)
		sum += p1[i] * p2[i];

	return sum;
}

#ifdef THREAD
void merge(Range* arg){
	int l = arg->l;
	int idx_l = arg->l;
	int mid = (arg->l + arg->r)/2;
	int idx_r = mid + 1;
	int r = arg->r;
	int len = 0;
	int which = arg->reqid;

	double *score = (double*)malloc(sizeof(double) * (r-l+1));
	char **movie_name = (char**)malloc(sizeof(char*) * (r-l+1));

	while(idx_l <= mid && idx_r <= r){
		if(data[which].score[idx_l] > data[which].score[idx_r] || (data[which].score[idx_l] == data[which].score[idx_r] && strcmp(data[which].movie_name[idx_l], data[which].movie_name[idx_r]) < 0)){
			score[len] = data[which].score[idx_l];
			movie_name[len] = data[which].movie_name[idx_l];
			len++;
			idx_l++;
		}
		else{
			score[len] = data[which].score[idx_r];
			movie_name[len] = data[which].movie_name[idx_r];
			len++;
			idx_r++;
		}
	}

	while(idx_l <= mid){
		score[len] = data[which].score[idx_l];
		movie_name[len] = data[which].movie_name[idx_l];
		len++;
		idx_l++;
	}

	while(idx_r <= r){
		score[len] = data[which].score[idx_r];
		movie_name[len] = data[which].movie_name[idx_r];
		len++;
		idx_r++;
	}

	memcpy(data[which].score + l, score, sizeof(double) * (r-l+1));
	memcpy(data[which].movie_name + l, movie_name, sizeof(char*) * (r-l+1));

	free(score);
	free(movie_name);
}

void* mergesort(void* arg){

	Range *range = (Range*)arg;
	int l = range->l;
	int r = range->r;
	int mid = (l + r) / 2;
	int level = range->level;
	int which = range->reqid;
	int lid = range->id * 2;
	int rid = range->id * 2 + 1;

	if(l > r) return NULL;


	Range r1 = {l, mid, level + 1, which, lid};
	Range r2 = {mid + 1, r, level + 1, which, rid};

	if(r - l + 1 <= SIZE_LIMIT){
		sort(data[which].movie_name + l, data[which].score + l, r-l+1);
	}
	else if(level >= MAX_LEVEL){
		mergesort(&r1);
		mergesort(&r2);
		merge(range);
	}
	else{

		pthread_t myt;
		pthread_create(&myt, NULL, mergesort, &r2);
		mergesort(&r1);
		
		pthread_join(myt, NULL);
	
		merge(range);
	}
}

#elif defined PROCESS
void merge(Range* arg, int l, int r, int level){
	int idx_l = l;
	int mid = (l + r) / 2;
	int idx_r = mid + 1;
	int len = 0;
	Range *temp = (Range*)malloc(sizeof(Range));

	while(idx_l <= mid && idx_r <= r){
		if(arg->score[idx_l] > arg->score[idx_r] || (arg->score[idx_l] == arg->score[idx_r] && strcmp(arg->movie_name[idx_l], arg->movie_name[idx_r]) < 0)){
			temp->score[len] = arg->score[idx_l];
			temp->movie_name[len] = arg->movie_name[idx_l];
			len++;
			idx_l++;
		}
		else{
			temp->score[len] = arg->score[idx_r];
			temp->movie_name[len] = arg->movie_name[idx_r];
			len++;
			idx_r++;
		}
	}

	while(idx_l <= mid){
		temp->score[len] = arg->score[idx_l];
		temp->movie_name[len] = arg->movie_name[idx_l];
		len++;
		idx_l++;
	}

	while(idx_r <= r){
		temp->score[len] = arg->score[idx_r];
		temp->movie_name[len] = arg->movie_name[idx_r];
		len++;
		idx_r++;
	}


	for(int i = l, idx = 0; i <= r; i++, idx++){
		arg->score[i] = temp->score[idx];
		arg->movie_name[i] = temp->movie_name[idx];
	}
	free(temp);
}

void mergesort(Range* arg, int l, int r, int level){
	if(l > r) return;

	if(level == MAX_LEVEL){
		Range *temp = (Range*)malloc(sizeof(Range));
		memcpy(temp->movie_name, arg->movie_name, sizeof(arg->movie_name));
		memcpy(temp->score, arg->score, sizeof(arg->score));

		sort(&temp->movie_name[l], &temp->score[l], r - l + 1);

		for(int i = l; i <= r; i++){
			arg->score[i] = temp->score[i];
			strcpy(arg->movie_name[i], temp->movie_name[i]);
		}
	}
	else{
		int mid = (l + r) / 2;
		pid_t pid = fork();

		if(pid == 0){
			mergesort(arg, l, mid, level+1);
			exit(0);
		}

		mergesort(arg, mid+1, r, level+1);
		waitpid(pid, NULL, 0);
		merge(arg, l, r, level);
	}
}
#endif

void* deal_request(void* arg){
	MYREQ myreq = *(MYREQ*)arg;
	int which = myreq.which;
	int len = 0;

	#ifdef THREAD
	Range *range = (Range*)malloc(sizeof(Range));
	#elif defined PROCESS
	Range *range = mylloc(sizeof(Range));
	#endif

	for(int i = 0; i < num_of_movies; i++){
		if(!strcmp(reqs[which]->keywords, "*") || strstr(movies[i]->title, reqs[which]->keywords)){
			#ifdef THREAD
			data[which].movie_name[len] = movies[i]->title;
			data[which].score[len] = cross(movies[i]->profile, reqs[which]->profile);
			len++;
			#elif defined PROCESS
			range->movie_name[len] = mylloc(sizeof(char) * MAX_LEN);
			strcpy(range->movie_name[len], (*movies[i]).title);
			range->score[len] = cross((*movies[i]).profile, reqs[which]->profile);
			len++;
			#endif
		}
	}
	
	#ifdef THREAD
	range->l = 0;
	range->r = len - 1;
	range->level = 0;
	range->reqid = which;
	range->id = 1;

	pthread_create(&tid[which][1], NULL, mergesort, range);
	pthread_join(tid[which][1], NULL);
	#elif defined PROCESS
	mergesort(range, 0, len - 1, 0);
	#endif
	
	char buf[MAX_LEN];
	#ifdef THREAD
	sprintf(buf, "%dt.out", reqs[which]->id);
	FILE *fd = fopen(buf, "w+");
	for(int i = 0; i < len; i++)
		fprintf(fd, "%s\n", data[which].movie_name[i]);
	free(range);
	pthread_exit(NULL);
	#elif defined PROCESS
	sprintf(buf, "%dp.out", reqs[which]->id);
	FILE *fd = fopen(buf, "w+");
	for(int i = 0; i < len; i++)
		fprintf(fd, "%s\n", range->movie_name[i]);
	#endif
}

int main(int argc, char *argv[]){

	if(argc != 1){
#ifdef PROCESS
		fprintf(stderr,"usage: ./pserver\n");
#elif defined THREAD
		fprintf(stderr,"usage: ./tserver\n");
#endif
		exit(-1);
	}

	FILE *fp;

	if ((fp = fopen("./data/movies.txt","r")) == NULL){
		ERR_EXIT("fopen");
	}

	initialize(fp);
	assert(fp != NULL);
	
	#ifdef THREAD
	data = (Data*)malloc(sizeof(Data) * (num_of_reqs));

	for(int t = 0; t < num_of_reqs; t+=REQ_THREAD){
		MYREQ myreq[REQ_THREAD];

		int i;
		for(i = 0; i < REQ_THREAD && t + i < num_of_reqs; i++){
			myreq[i].which = t + i;
			pthread_create(&tid[t+i][0], NULL, deal_request, &myreq[i]);
		}

		for(int j = 0; j < i; j++)
			pthread_join(tid[t+j][0], NULL);
	}
	#elif defined PROCESS
	MYREQ myreq;
	myreq.which = 0;
	deal_request(&myreq);
	#endif

	fclose(fp);	

	return 0;
}

/**=======================================
 * You don't need to modify following code *
 * But feel free if needed.                *
 =========================================**/

request* read_request(){
	int id;
	char buf1[MAX_LEN],buf2[MAX_LEN];
	char delim[2] = ",";

	char *keywords;
	char *token, *ref_pts;
	char *ptr;
	double ret,sum;

	scanf("%u %254s %254s",&id,buf1,buf2);
	keywords = malloc(sizeof(char)*strlen(buf1)+1);
	if(keywords == NULL){
		ERR_EXIT("malloc");
	}

	memcpy(keywords, buf1, strlen(buf1));
	keywords[strlen(buf1)] = '\0';

	double* profile = malloc(sizeof(double)*NUM_OF_GENRE);
	if(profile == NULL){
		ERR_EXIT("malloc");
	}
	sum = 0;
	ref_pts = strtok(buf2,delim);
	for(int i = 0;i < NUM_OF_GENRE;i++){
		ret = strtod(ref_pts, &ptr);
		profile[i] = ret;
		sum += ret*ret;
		ref_pts = strtok(NULL,delim);
	}

	// normalize
	sum = sqrt(sum);
	for(int i = 0;i < NUM_OF_GENRE; i++){
		if(sum == 0)
				profile[i] = 0;
		else
				profile[i] /= sum;
	}

	request* r = malloc(sizeof(request));
	if(r == NULL){
		ERR_EXIT("malloc");
	}

	r->id = id;
	r->keywords = keywords;
	r->profile = profile;

	return r;
}

/*=================initialize the dataset=================*/
void initialize(FILE* fp){

	char chunk[MAX_LEN] = {0};
	char *token,*ptr;
	double ret,sum;
	int cnt = 0;

	assert(fp != NULL);

	// first row
	if(fgets(chunk,sizeof(chunk),fp) == NULL){
		ERR_EXIT("fgets");
	}

	memset(movies,0,sizeof(movie_profile*)*MAX_MOVIES);	

	while(fgets(chunk,sizeof(chunk),fp) != NULL){
		
		assert(cnt < MAX_MOVIES);
		chunk[MAX_LEN-1] = '\0';

		const char delim1[2] = " "; 
		const char delim2[2] = "{";
		const char delim3[2] = ",";
		unsigned int movieId;
		movieId = atoi(strtok(chunk,delim1));

		// title
		token = strtok(NULL,delim2);
		char* title = malloc(sizeof(char)*strlen(token)+1);
		if(title == NULL){
			ERR_EXIT("malloc");
		}
		
		// title.strip()
		memcpy(title, token, strlen(token)-1);
	 	title[strlen(token)-1] = '\0';

		// genres
		double* profile = malloc(sizeof(double)*NUM_OF_GENRE);
		if(profile == NULL){
			ERR_EXIT("malloc");
		}

		sum = 0;
		token = strtok(NULL,delim3);
		for(int i = 0; i < NUM_OF_GENRE; i++){
			ret = strtod(token, &ptr);
			profile[i] = ret;
			sum += ret*ret;
			token = strtok(NULL,delim3);
		}

		// normalize
		sum = sqrt(sum);
		for(int i = 0; i < NUM_OF_GENRE; i++){
			if(sum == 0)
				profile[i] = 0;
			else
				profile[i] /= sum;
		}

		movie_profile* m = malloc(sizeof(movie_profile));
		if(m == NULL){
			ERR_EXIT("malloc");
		}

		m->movieId = movieId;
		m->title = title;
		m->profile = profile;

		movies[cnt++] = m;
	}
	num_of_movies = cnt;

	// request
	scanf("%d",&num_of_reqs);
	assert(num_of_reqs <= MAX_REQ);
	for(int i = 0; i < num_of_reqs; i++){
		reqs[i] = read_request();
	}
}
/*========================================================*/
