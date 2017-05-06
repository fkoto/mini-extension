#include <stdio.h>

typedef struct contig{
	char name[100];
	struct contig *next;
}contiguous;

contiguous *start;
contiguous *end = NULL;

int name_counter = 0;
const char* create_new_name(){
	char *res = (char*) malloc(17*sizeof(char));
	
	snprintf(res, 17, "mini_internal_%d", name_counter);
	
	name_counter++;
	return (const char*) res;
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
