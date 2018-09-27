#include <stdio.h>

typedef struct contig{
	char name[100];
	struct contig *next;
}contiguous;

contiguous *start;
contiguous *end = NULL;

typedef struct comm{
	int cnt;
	int local_rank;
	char name[100];
	struct comm *next;
}communicator;

communicator *start_comm;
communicator *end_comm = NULL;

typedef struct mpi_request_meta{
	int id;
	int count;
	int size;
	int data_code;
	char comm_name[100];
	struct mpi_request_meta *next;
}mpi_request_metadata;

mpi_request_metadata *start_req;
mpi_request_metadata *end_req = NULL;

int name_counter = 0;
const char* create_new_contig_name(){
	char *res = (char*) malloc(17*sizeof(char));
	
	snprintf(res, 17, "mini_internal_%d", name_counter);
	
	name_counter++;
	return (const char*) res;
}

int comm_name_counter = 0;
void create_new_comm_name(int color, char *name){
	snprintf(name, 16, "mini_comm_%d_%d", comm_name_counter, color);
	
	comm_name_counter++;
	return;
}

void insert_contig(char *name){
	contiguous *temp = (contiguous*) malloc(sizeof(contiguous));
	strcpy(temp->name, name);
	temp->next = NULL;
	if (end == NULL){
		start = temp;
		end = temp;
	}
	else{
		end->next = temp;
		end = temp;
	}
	//	printf("Inserted contig with name %s\n", name);
}

void delete_contig_list(){
	contiguous *temp = start;
	contiguous *temp2;

	while(temp != NULL){
		temp2 = temp;
		temp = temp->next;
		free(temp2);
	}
}

int find_contig(char *name){
	contiguous *temp = start;
	//	printf("Searching for contig %s\n",name);

	while(temp != NULL){
		//		printf("Comparing with %s\n", temp->name);
		if (strcmp(temp->name, name) == 0){
			return 1;
		}
		temp = temp->next;
	}
	return 0;
}

void insert_comm(char *name, int local_rank){
	communicator *temp = (communicator*) malloc(sizeof(communicator));
	strcpy(temp->name, name);
	temp->cnt = 1;//!!!!!NOT 0 in order to be aligned with parser!!!!!
	temp->local_rank = local_rank;
	temp->next = NULL;
	if (end_comm == NULL){
		start_comm = temp;
		end_comm = temp;
	}
	else{
		end_comm->next = temp;
		end_comm = temp;
	}
}

void delete_comm(char *name){
	communicator *temp= start_comm;
	
	if (strcmp(temp->name, name) == 0){//delete head
		start_comm = start_comm->next;
		free(temp);
		return;
	}
	
	while (temp->next != NULL){
		if (strcmp(temp->next->name, name) == 0){
			communicator *temp2 = temp->next;
			temp->next = temp2->next;
			free(temp2);
			return;
		}
		temp = temp->next;
	}
}

void delete_comm_list(){
	communicator *temp = start_comm;
	communicator *temp2;

	while(temp != NULL){
		temp2 = temp;
		temp = temp->next;
		free(temp2);
	}
}

int find_comm(char *name){
	communicator *temp = start_comm;

	while(temp != NULL){
		if (strcmp(temp->name, name) == 0){
			return 1;
		}
		temp = temp->next;
	}
	return 0;
}

/*
 * This returns the local rank of the calling process in 
 * this communicator. If not found it returns a negative number.
*/
int find_comm_rank(char *name){
	communicator *temp = start_comm;

	while(temp != NULL){
		if (strcmp(temp->name, name) == 0){
			return temp->local_rank;
		}
		temp = temp->next;
	}
	return -1;
}

int get_comm_cnt_and_incr(char *name){
	communicator *temp = start_comm;

	while(temp != NULL){
		if (strcmp(temp->name, name) == 0){
			int x = temp->cnt;
			temp->cnt = temp->cnt + 1;
			return x;
		}
		temp = temp->next;
	}
	return -1;
}

void update_comm_name(char *oldname, char *newname){
	communicator *temp = start_comm;
	
	while (temp != NULL){
		if (strcmp(temp->name, oldname) == 0){
			break;
		}
		temp = temp->next;
	}
	strcpy(temp->name, newname);
}

void insert_req(MPI_Request *req, int cnt, int size, int code, char *name){
	mpi_request_metadata *temp = (mpi_request_metadata*) malloc(sizeof(mpi_request_metadata));

	memcpy(&temp->id, req, sizeof(int));

	temp->count = cnt;
	temp->size = size;
	temp->data_code = code;
	strcpy(temp->comm_name, name);
	temp->next = NULL;
	
	if (end_req == NULL){
		start_req = temp;
		end_req = temp;
	}
	else{
		end_req->next = temp;
		end_req = temp;
	}
}

void delete_req(MPI_Request *req){
	mpi_request_metadata *temp= start_req;

	if (memcmp(&temp->id, req, sizeof(int)) == 0){//delete head
		start_req = start_req->next;
		free(temp);
		return;
	}
	
	while (temp->next != NULL){
		if (memcmp(&temp->id, req, sizeof(int)) == 0){
			mpi_request_metadata *temp2 = temp->next;
			temp->next = temp2->next;
			free(temp2);
			return;
		}
		temp = temp->next;
	}
}

void delete_req_list(){
	mpi_request_metadata *temp = start_req;
	mpi_request_metadata *temp2;
	while(temp != NULL){
		temp2 = temp;
		temp = temp->next;
		free(temp2);
	}
}

mpi_request_metadata* find_req(MPI_Request *req){
	mpi_request_metadata *temp = start_req;
	
	while(temp != NULL){
		if (memcmp(temp, req, sizeof(int)) == 0){
			return temp;
		}
		temp = temp->next;
	}
	return NULL;
}



void merge(int *arr, int l1, int h1, int l2, int h2){
	int *temp;
	int length = (h1 - l1) + (h2 - l2) + 2;

	temp = (int*) malloc(length * sizeof(int));	//allocate temp buffer

	int iter1, iter2, iter_t, i;

	iter1 = l1;	//set iterators
	iter2 = l2;
	iter_t = 0;
	
	while((iter1 <= h1) && (iter2 <= h2)){	//while both subarrays have values, compare and merge
		if (arr[iter1] <= arr[iter2]){
			temp[iter_t] = arr[iter1];
			iter1++;
		}
		else{
			temp[iter_t] = arr[iter2];
			iter2++;
		}
		iter_t++;
	}

	if (iter1 > h1){	//complete with rest elements
		for (i = iter2; i <= h2; i++){
			temp[iter_t] = arr[i];
			iter_t++;
		}
	}
	else{
		for(i = iter1; i <= h1; i++){
			temp[iter_t] = arr[i];
			iter_t++;
		}
	}
	
	iter_t = 0;
	for (i = l1; i <= h2; i++){	//copy back from temp buffer
		arr[i] = temp[iter_t];
		iter_t++;
	}

	free(temp);
}

void mergesort(int *arr, int low, int high){

	int mid;
	if (low < high){
		mid = (low + high)/2;
		mergesort(arr, low, mid);	//split
		mergesort(arr, mid+1, high);
		merge(arr, low, mid, mid+1, high);	//merge
	}
}

int max(int *arr, int size){
	int result = arr[0];
	int i;

	for(i = 1; i < size; i++){
		if(arr[i] > result){
			result = arr[i];
		}
	}

	return result;
}

int min(int *arr, int size){
	int result = arr[0];
	int i;

	for(i = 1; i < size; i++){
		if(arr[i] < result){
			result = arr[i];
		}
	}

	return result;
}

float median(int *arr, int size){
	float result;
	int i;
	int* temp = (int*) malloc(size * sizeof(int));
	
	for(i = 0; i < size; i++){
		temp[i] = arr[i];
	}

	mergesort(temp, 0 ,size - 1);

	if ((size % 2) == 0){
		result = (temp[size/2] + temp[size/2 - 1]) / 2;//!!!!!!!!
	}
	else{
		result = temp[size/2] * 1.0;//!!!!!!!
	}

	free(temp);

	return result;
}
