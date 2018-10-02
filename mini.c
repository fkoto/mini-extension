#ifdef MPI_BUILD_PROFILING
#undef MPI_BUILD_PROFILING
#endif
#include <stdio.h>

#define WITH_MPI

#ifdef PAPI
#include <papi.h>
#else
#define PAPI_OK 0
long long PAPI_accum_counters(long long values[], int x){
	return PAPI_OK;
}

long long PAPI_get_real_usec(){
	return (long long) 0;
}
#endif

#include <mpi.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#if defined(NEEDS_STDLIB_PROTOTYPES)
#include "protofix.h"
#endif

//#define PAPI	//run with PAPI enabled

void papi_get_start_measurement();
void papi_get_end_measurement();
void papi_print_compute(char msg[], int llrank);
void print_op(MPI_Op op);
int compare_comms(MPI_Comm comm1, MPI_Comm comm2);

//!!!!!!!!!!!!!!!!!!!!!!!!!!!MY STRUCTS!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
#include "utils.c"
//!!!!!!!!!!!!!!!!!!!!!!!!!!!MY STRUCTS!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

struct stat st = {0};
int MINI_Trace_hasBeenInit = 0;
int MINI_Trace_hasBeenFinished = 0;     
int global;

FILE *fp;
float real_time, proc_time;
long long ins1, ins2,t1,t2;
#ifdef PAPI
int EventSet=PAPI_NULL;
#endif
int Events[30],i_mode=0,en_time=0,i_counter=0;
int num_hwcntrs=0,bcount=0,imod=0,glob_size=0,glob_np=0,buff=250;
long long values[1],start_time,end_time,elapsed_time;
char longmsg[120000],temp_buf[100],temp_long[1000];
char *testt;
#define TRACE_PRINTF(msg) \
	if ( (MINI_Trace_hasBeenInit) && (!MINI_Trace_hasBeenFinished) ) {\
		PMPI_Comm_rank( MPI_COMM_WORLD, &llrank ); \
		printf( "%s\n", msg ); \
		fflush( stdout ); \
	}

/** Convention: return values <= 100 represent contiguous datatypes.
* return values >100 represent non contiguous datatypes (for now only 101).
**/
int encode_datatype(const char *dat) {
	//	printf("encode datatype started with %s\n", dat);
	int res=-1;
	if(strcmp("MPI_DOUBLE_PRECISION",dat)==0 || strcmp("MPI_DOUBLE",dat)==0) {
		res=0;
	}
	else if(strcmp("MPI_INTEGER",dat)==0 || strcmp("MPI_INT",dat)==0) {
		res=1;
	}
	else if(strcmp("MPI_CHARACTER",dat)==0 || strcmp("MPI_CHAR",dat)==0) {
		res=2;
	}
	else if(strcmp("MPI_SHORT",dat)==0) {
		res=3;
	}
	else if(strcmp("MPI_LONG",dat)==0) {
		res=4;
	}
	else if(strcmp("MPI_REAL",dat)==0 || strcmp("MPI_FLOAT",dat)==0) {
		res=5;
	}
	else if(strcmp("MPI_BYTE",dat)==0) {
		res=6;
	}
	else if(find_contig((char *) dat)){
		res = 100;
	}
	else{
		res = 101;
	}
	//	printf(", returning: %d\n",res); 
	return res;
}



int   MPI_Allreduce( sendbuf, recvbuf, count, datatype, op, comm )
const void * sendbuf;
void * recvbuf;
int count;
MPI_Datatype datatype;
MPI_Op op;
MPI_Comm comm;
{
	int   returnVal=0;
	int llrank;
	char msg[100];
	int np;
	char nam[MPI_MAX_OBJECT_NAME];
	char nam_comm[MPI_MAX_OBJECT_NAME];
	int resultlen;
	int comm_id;
#ifdef PAPI
	papi_get_start_measurement();
#endif

	PMPI_Type_get_name(datatype,nam,&np);

	np=encode_datatype((const char*)&nam);

	PMPI_Comm_rank( MPI_COMM_WORLD, &llrank );
#ifdef PAPI
	papi_print_compute(msg, llrank);
#endif

	if (bcount>buff )
	{
		fprintf(fp,"%s", longmsg);
		longmsg[0]='\0';
		bcount=0;
	}
#ifdef PAPI
	strcat(longmsg,msg);
#endif
	//  sprintf(msg, "%d comm_size %d\n",llrank,global);//?????????????
	//  strcat(longmsg,msg);


	bcount=count+2;
#ifdef PAPI
	if(PAPI_accum_counters(values, 1)!=PAPI_OK) printf("This PAPI event is not supported\n");
	ins2=values[0];
#endif

#ifdef WITH_MPI
	returnVal = PMPI_Allreduce( sendbuf, recvbuf, count, datatype, op, comm );
#endif

#ifdef PAPI
	PAPI_accum_counters(values, 1);

	ins1=values[0];
#endif
	int size;
	PMPI_Type_size(datatype, &size);
#ifdef PAPI
	sprintf(msg, "%d allReduce %d %lld %d (of %d bytes)",llrank,count,ins1-ins2,np, size);
#else
	sprintf(msg, "%d allReduce %d (of %d bytes)",llrank,count, size);
#endif
	strcat(longmsg, msg);
	
//	print_op(op);

	sprintf(msg, " of type %d", np);
	strcat(longmsg, msg);

	PMPI_Comm_get_name(comm, nam_comm, &resultlen);
	comm_id = get_comm_cnt_and_incr(nam_comm);
	sprintf(msg, " on comm %s %d\n", nam_comm, comm_id);
	strcat(longmsg, msg);

	bcount=bcount+3;

#ifdef PAPI
	papi_get_end_measurement();
#endif
	return returnVal;
}

int  MPI_Gather( sendbuf, sendcount, sendtype, recvbuf, recvcnt, recvtype, root, comm )
const void * sendbuf;
int sendcount;
MPI_Datatype sendtype;
void * recvbuf;
int recvcnt;
MPI_Datatype recvtype;
int root;
MPI_Comm comm;
{
	int  returnVal=0;
	int llrank, local;
	int np,np2;
	char msg[300];
	char nam[MPI_MAX_OBJECT_NAME];
	char nam_comm[MPI_MAX_OBJECT_NAME];
	int resultlen;

#ifdef PAPI
	papi_get_start_measurement();
#endif
	PMPI_Comm_rank( MPI_COMM_WORLD, &llrank );

#ifdef PAPI
	if(en_time==1) {
		end_time=PAPI_get_real_usec();
		sprintf(msg,"%d compute %lld %.6f\n",llrank,values[0]-ins1,(double)(end_time-start_time)/1000000);
	}
	else sprintf(msg,"%d compute %lld\n",llrank,values[0]-ins1);
#endif
	PMPI_Type_get_name(sendtype,nam,&np);
	np=encode_datatype((const char*)&nam);

	PMPI_Type_get_name(recvtype,nam,&np2);
	np2=encode_datatype((const char*)&nam);

	if (bcount>buff )
	{
		fprintf(fp, "%s", longmsg);
		longmsg[0]='\0';
		bcount=0;
	}
#ifdef PAPI
	strcat(longmsg,msg);
#endif
	int ssize, rsize;
	PMPI_Type_size(sendtype, &ssize);
	PMPI_Type_size(recvtype, &rsize);

	sprintf(msg, "%d gather %d (of %d bytes) %d",
	llrank,sendcount, ssize, root);
	strcat(longmsg,msg);

	if (np == np2){
		sprintf(msg, " of type %d", np);
	}
	else{
		sprintf(msg, " of types %d, %d", np, np2);
	}
	strcat(longmsg,msg);
	
	PMPI_Comm_get_name(comm, nam_comm, &resultlen);
	if (comm == MPI_COMM_WORLD){
		sprintf(msg, " on comm %s\n", nam_comm);
		strcat(longmsg, msg);
	}else{
		local = find_comm_rank(nam_comm);
		sprintf(msg, " on comm %s %d\n", nam_comm, local);
		strcat(longmsg, msg);
	}

#ifdef WITH_MPI
	returnVal = PMPI_Gather( sendbuf, sendcount, sendtype, recvbuf, recvcnt,
	recvtype, root,comm );
#endif

	bcount=bcount+2;

#ifdef PAPI
	papi_get_end_measurement();
#endif
	return returnVal;
}

int  MPI_Alltoall( sendbuf, sendcount, sendtype, recvbuf, recvcnt, recvtype, comm )
const void * sendbuf;
int sendcount;
MPI_Datatype sendtype;
void * recvbuf;
int recvcnt;
MPI_Datatype recvtype;
MPI_Comm comm;
{
	int  returnVal=0;
	int llrank;
	int np,np2;
	char msg[300];
	char nam[MPI_MAX_OBJECT_NAME];
	char nam_comm[MPI_MAX_OBJECT_NAME];
	int resultlen;
#ifdef PAPI
	papi_get_start_measurement();
#endif
	PMPI_Type_get_name(sendtype,nam,&np);
	np=encode_datatype((const char*)&nam);
	PMPI_Type_get_name(recvtype,nam,&np2);
	np2=encode_datatype((const char*)&nam);

	PMPI_Comm_rank( MPI_COMM_WORLD, &llrank );
#ifdef PAPI
	if(en_time==1) {
		end_time=PAPI_get_real_usec();
		sprintf(msg,"%d compute %lld %.6f\n",llrank,values[0]-ins1,(double)(end_time-start_time)/1000000);
	}
	else sprintf(msg,"%d compute %lld\n",llrank,values[0]-ins1);
#endif
	if (bcount>buff )
	{
		fprintf(fp,"%s", longmsg);
		longmsg[0]='\0';
		bcount=0;
	}
#ifdef PAPI
	strcat(longmsg,msg);
#endif
	int ssize, rsize;
	PMPI_Type_size(sendtype, &ssize);
	PMPI_Type_size(recvtype, &rsize);

	sprintf(msg, "%d allToAll %d (of %d bytes)",
	llrank,sendcount, ssize);
	strcat(longmsg,msg);

	if (np == np2){
		sprintf(msg, " of type %d", np);
	}
	else{
		sprintf(msg, " of types %d, %d", np, np2);
	}
	strcat(longmsg,msg);
	
	PMPI_Comm_get_name(comm, nam_comm, &resultlen);
	int comm_id = get_comm_cnt_and_incr(nam_comm);
	sprintf(msg, " on comm %s %d\n", nam_comm, comm_id);
	strcat(longmsg,msg);

	bcount = bcount + 3;
	
#ifdef WITH_MPI
	returnVal = PMPI_Alltoall( sendbuf, sendcount, sendtype, recvbuf, recvcnt, 
	recvtype, comm );
#endif

#ifdef PAPI
	papi_get_end_measurement();
#endif
	return returnVal;
}

int   MPI_Gatherv( sendbuf, sendcnts, sendtype, recvbuf, recvcnts, displs, recvtype, root, comm )
const void * sendbuf;
int sendcnts;
MPI_Datatype sendtype;
void * recvbuf;
const int recvcnts[];
const int displs[];
MPI_Datatype recvtype;
int root;
MPI_Comm comm;
{
	int   returnVal=0;
	int llrank, local;
	char msg[300];
	int max_recv=0,min_recv=0, size;//,i,s_buffer=0; //unused
	float median_recv = 0.0;
	int np,np2;
	char nam[MPI_MAX_OBJECT_NAME];
	char nam_comm[MPI_MAX_OBJECT_NAME];
	int resultlen;

#ifdef PAPI
	papi_get_start_measurement();
#endif
	PMPI_Type_get_name(sendtype,nam,&np);
	np=encode_datatype((const char*)&nam);
	PMPI_Type_get_name(recvtype,nam,&np2);
	np2=encode_datatype((const char*)&nam);
	
	PMPI_Comm_rank( MPI_COMM_WORLD, &llrank );

	if (bcount>buff )
	{
		fprintf(fp,"%s", longmsg);
		longmsg[0]='\0';
		bcount=0;
	}
#ifdef PAPI
	papi_print_compute(msg, llrank);
	strcat(longmsg,msg);
#endif
	size = sizeof(recvcnts)/sizeof(recvcnts[0]);
	max_recv = max((int*)recvcnts, size);
	min_recv = min((int*)recvcnts, size);
	median_recv = median((int*)recvcnts, size);

	int rsize;
	PMPI_Type_size(recvtype, &rsize);

	sprintf(msg, "%d gatherv min=%d median=%f max=%d (of %d bytes) %d", 
	llrank,min_recv,median_recv,max_recv, rsize, root);

	strcat(longmsg,msg);

	if (np != np2){
		sprintf(msg, " of types %d, %d",np,np2);
	}
	else{
		sprintf(msg, " of type %d", np);
	}
	strcat (longmsg,msg);

	PMPI_Comm_get_name(comm, nam_comm, &resultlen);
	if (comm == MPI_COMM_WORLD){
		sprintf(msg, " on comm %s\n", nam_comm);
		strcat(longmsg, msg);
	}else{
		local = find_comm_rank(nam_comm);
		sprintf(msg, " on comm %s %d\n", nam_comm, local);
		strcat(longmsg, msg);
	}

	bcount = bcount + 4;

#ifdef WITH_MPI
	returnVal = PMPI_Gatherv( sendbuf, sendcnts, sendtype, recvbuf, recvcnts,
	displs, recvtype, root, comm );
#endif

#ifdef PAPI
	papi_get_end_measurement();
#endif
	return returnVal;
}

int   MPI_Allgather( sendbuf, sendcnts, sendtype, recvbuf, recvcnts, recvtype, comm )
const void * sendbuf;
int sendcnts;
MPI_Datatype sendtype;
void * recvbuf;
int recvcnts;
MPI_Datatype recvtype;
MPI_Comm comm;
{
	int   returnVal=0;
	int llrank;
	char msg[300];
	//int i;//unused
	int np,np2;
	char nam[MPI_MAX_OBJECT_NAME];
	char nam_comm[MPI_MAX_OBJECT_NAME];
	int resultlen;
#ifdef PAPI
	papi_get_start_measurement();
#endif
	PMPI_Type_get_name(sendtype,nam,&np);
	np=encode_datatype((const char*)&nam);
	PMPI_Type_get_name(recvtype,nam,&np2);
	np2=encode_datatype((const char*)&nam);


	PMPI_Comm_rank( MPI_COMM_WORLD, &llrank );

	if (bcount>buff )
	{
		fprintf(fp, "%s", longmsg);
		longmsg[0]='\0';
		bcount=0;
	}
#ifdef PAPI
	papi_print_compute(msg, llrank);
	strcat(longmsg,msg);
#endif
	int ssize, rsize;
	PMPI_Type_size(sendtype, &ssize);
	PMPI_Type_size(recvtype, &rsize);
	sprintf(msg, "%d allGather %d (of %d bytes)", llrank,sendcnts, ssize);
	strcat(longmsg,msg);

	if (np == np2){
		sprintf(msg, " of type %d", np);
	}
	else {
		sprintf(msg, " of types %d, %d", np, np2);
	}
	strcat(longmsg,msg);

	sprintf(msg, " %d (of %d bytes)", recvcnts, rsize); 
	strcat(longmsg, msg);//receive traces
	
	PMPI_Comm_get_name(comm, nam_comm, &resultlen);
	int comm_id = get_comm_cnt_and_incr(nam_comm);
	sprintf(msg, " on comm %s %d\n", nam_comm, comm_id);
	strcat(longmsg,msg);

	bcount=bcount+3;

#ifdef WITH_MPI
	returnVal = PMPI_Allgather( sendbuf, sendcnts, sendtype, recvbuf,
	recvcnts, recvtype, comm );
#endif

#ifdef PAPI
	papi_get_end_measurement();
#endif
	return returnVal;
}

int   MPI_Allgatherv( sendbuf, sendcnts, sendtype, recvbuf, recvcnts, displs, recvtype, comm )
const void * sendbuf;
int sendcnts;
MPI_Datatype sendtype;
void * recvbuf;
const int recvcnts[];
const int displs[];
MPI_Datatype recvtype;
MPI_Comm comm;
{
	int   returnVal=0;
	int llrank;
	char msg[300];
	int max_recv=0,min_recv=0,size=0;//,s_buffer=0,r_buffer=0,i; //unused
	float median_recv=0.0;
	int np,np2;
	char nam[MPI_MAX_OBJECT_NAME];
	char nam_comm[MPI_MAX_OBJECT_NAME];
	int resultlen;
#ifdef PAPI
	papi_get_start_measurement();
#endif

	PMPI_Type_get_name(sendtype,nam,&np);
	np=encode_datatype((const char*)&nam);
	PMPI_Type_get_name(recvtype,nam,&np2);
	np2=encode_datatype((const char*)&nam);


	PMPI_Comm_rank( MPI_COMM_WORLD, &llrank );

	if (bcount>buff )
	{
		fprintf(fp, "%s", longmsg);
		longmsg[0]='\0';
		bcount=0;
	}
#ifdef PAPI
	papi_print_compute(msg, llrank);
	strcat(longmsg,msg);
#endif

	size = sizeof(recvcnts)/sizeof(recvcnts[0]);
	max_recv = max((int*)recvcnts, size);
	min_recv = min((int*)recvcnts, size);
	median_recv = median((int*)recvcnts, size);

	int ssize, rsize;
	PMPI_Type_size(sendtype, &ssize);
	PMPI_Type_size(recvtype, &rsize);


	sprintf(msg, "%d allgatherv min=%d median=%f max=%d", 
	llrank,min_recv,median_recv,max_recv);
	strcat(longmsg, msg);

	sprintf(msg, " (of %d bytes)", ssize);
	strcat(longmsg, msg);

	
	if (np != np2){
		sprintf(msg, " of types %d, %d",np,np2);
	}
	else{
		sprintf(msg, " of type %d", np);
	}
	strcat (longmsg,msg);

	PMPI_Comm_get_name(comm, nam_comm, &resultlen);
	int comm_id = get_comm_cnt_and_incr(nam_comm);
	sprintf(msg, " on comm %s %d\n",nam_comm, comm_id);
	strcat (longmsg,msg);
	
	bcount=bcount + 4;

#ifdef WITH_MPI
	returnVal = PMPI_Allgatherv( sendbuf, sendcnts, sendtype, recvbuf,
	recvcnts, displs, recvtype, comm );
#endif

#ifdef PAPI
	papi_get_end_measurement();
#endif
	return returnVal;
}

int   MPI_Reduce_scatter( sendbuf, recvbuf, recvcnts, datatype, op, comm )
const void * sendbuf;
void * recvbuf;
const int recvcnts[];
MPI_Datatype datatype;
MPI_Op op;
MPI_Comm comm;
{
	int   returnVal=0;
	int llrank;
	char msg[300];
	int np;
	char nam[MPI_MAX_OBJECT_NAME];
	char nam_comm[MPI_MAX_OBJECT_NAME];
	int resultlen;
	int csize,rmin,rmax;
	float rmedian;
#ifdef PAPI
	papi_get_start_measurement();
#endif
	PMPI_Type_get_name(datatype,nam,&np);
	np=encode_datatype((const char*)&nam);


	PMPI_Comm_rank( MPI_COMM_WORLD, &llrank );


	csize = sizeof(recvcnts)/sizeof(recvcnts[0]);
	rmax = max((int*)recvcnts, csize);
	rmin = min((int*)recvcnts, csize);
	rmedian = median((int*)recvcnts, csize);


	if (bcount>buff )
	{
		fprintf(fp, "%s", longmsg);
		longmsg[0]='\0';
		bcount=0;
	}
#ifdef PAPI
	papi_print_compute(msg, llrank);
	strcat(longmsg,msg);
#endif
	int size;
	PMPI_Type_size(datatype, &size); 

	sprintf(msg, "%d reduceScatter min=%d median=%f max=%d (of %d bytes) of type %d", llrank, rmin, rmedian, rmax, size, np);
	strcat(longmsg,msg); 
	
	PMPI_Comm_get_name(comm, nam_comm, &resultlen);
	int comm_id = get_comm_cnt_and_incr(nam_comm);
	sprintf(msg, " on comm %s %d\n", nam_comm, comm_id);
	strcat(longmsg, msg);

	bcount=bcount+2;
#ifdef PAPI
	PAPI_accum_counters(values, 1);
	ins2=values[0];
#endif

#ifdef WITH_MPI
	returnVal = PMPI_Reduce_scatter( sendbuf, recvbuf, recvcnts, datatype, op,
	comm );
#endif

#ifdef PAPI
	PAPI_accum_counters(values, 1);
	ins1=values[0];

	sprintf(msg,"%lld ",ins1-ins2);
	strcat(longmsg,msg);

	papi_get_end_measurement();
#endif
	return returnVal;
}


int   MPI_Alltoallv( sendbuf, sendcnts, sdispls, sendtype, recvbuf, recvcnts, rdispls, recvtype, comm )
const void * sendbuf;
const int sendcnts[];
const int sdispls[];
MPI_Datatype sendtype;
void * recvbuf;
const int recvcnts[];
const int rdispls[];
MPI_Datatype recvtype;
MPI_Comm comm;
{
	int   returnVal=0;
	int llrank;
	char msg[300];
	//int i,max_send=0,max_send_displ=0,max_recv=0,max_recv_displ=0,s_buffer=0,r_buffer=0;//unused
	int smin, smax, size, ssize;
	float smedian;
	int np,np2;
	char nam[MPI_MAX_OBJECT_NAME];
	char nam_comm[MPI_MAX_OBJECT_NAME];
	int resultlen;

	/*
	MPI_Alltoallv - prototyping replacement for MPI_Alltoallv
	Trace the beginning and ending of MPI_Alltoallv.
*/
#ifdef PAPI
	papi_get_start_measurement();
#endif
	PMPI_Type_get_name(sendtype,nam,&np);
	np=encode_datatype((const char*)&nam);
	PMPI_Type_get_name(recvtype,nam,&np2);
	np2=encode_datatype((const char*)&nam);

	PMPI_Type_size(sendtype, &ssize); 

	PMPI_Comm_rank( MPI_COMM_WORLD, &llrank );

	if (bcount>buff )
	{
		fprintf(fp, "%s", longmsg);
		longmsg[0]='\0';
		bcount=0;
	}
#ifdef PAPI
	papi_print_compute(msg, llrank);

	strcat(longmsg,msg);
#endif

	size = sizeof(recvcnts)/sizeof(recvcnts[0]);
	smax = max((int*)sendcnts, size);
	smin = min((int*)sendcnts, size);
	smedian = median((int*)sendcnts, size);

	sprintf(msg, "%d alltoAllv min=%d median=%f max=%d (of %d bytes)", llrank, smin, smedian, smax, ssize);
	strcat(longmsg, msg);

	if (np == np2){
		sprintf(msg, " of type %d", np);
	}
	else{
		sprintf(msg, " of types %d, %d", np, np2);
	}
	strcat(longmsg, msg);
	
	PMPI_Comm_get_name(comm, nam_comm, &resultlen);
	int comm_id = get_comm_cnt_and_incr(nam_comm);
	sprintf(msg, " on comm %s %d\n", nam_comm, comm_id);
	strcat(longmsg, msg);


	bcount=bcount+4;

#ifdef WITH_MPI
	returnVal = PMPI_Alltoallv( sendbuf, sendcnts, sdispls, sendtype, recvbuf, 
	recvcnts, rdispls, recvtype, comm );
#endif

#ifdef PAPI
	papi_get_end_measurement();
#endif
	return returnVal;
}

int   MPI_Barrier( comm )
MPI_Comm comm;
{
	int   returnVal=0;
	int llrank;
	char msg[100];
	char nam_comm[MPI_MAX_OBJECT_NAME];
	int resultlen;

#ifdef PAPI
	papi_get_start_measurement();
#endif
	PMPI_Comm_rank( MPI_COMM_WORLD, &llrank );
	if (bcount>buff )
	{
		fprintf(fp, "%s", longmsg);
		longmsg[0]='\0';
		bcount=0;
	}
#ifdef PAPI
	papi_print_compute(msg, llrank);
	strcat(longmsg,msg);
#endif
	sprintf(msg,"%d barrier",llrank);
	strcat(longmsg,msg);

	PMPI_Comm_get_name(comm, nam_comm, &resultlen);
	sprintf(msg, " on comm %s\n", nam_comm);
	strcat(longmsg,msg);

	bcount = bcount + 2;

#ifdef WITH_MPI
	returnVal = PMPI_Barrier( comm );
#endif

#ifdef PAPI
	papi_get_end_measurement();
#endif
	return returnVal;
}

int   MPI_Bcast( buffer, count, datatype, root, comm )
void * buffer;
int count;
MPI_Datatype datatype;
int root;
MPI_Comm comm;
{
	int   returnVal=0;
	int llrank, local;
	char msg[100];
	int np, resultlen;
	char nam[MPI_MAX_OBJECT_NAME];
	char nam_comm[MPI_MAX_OBJECT_NAME];
	PMPI_Type_get_name(datatype,nam,&np);
	np=encode_datatype((const char*)&nam);
#ifdef PAPI
	papi_get_start_measurement();
#endif
	PMPI_Comm_rank( MPI_COMM_WORLD, &llrank );
	if (bcount>buff )
	{
		fprintf(fp, "%s", longmsg);
		longmsg[0]='\0';
		bcount=0;
	}
	msg[0]='\0';
#ifdef PAPI
	papi_print_compute(msg, llrank);
	strcat(longmsg,msg);
#endif
		
	int size;
	PMPI_Type_size(datatype, &size);
	
	sprintf(msg, "%d bcast %d (of %d bytes) %d of type %d",llrank,count,size,root,np);
	strcat(longmsg,msg);

	PMPI_Comm_get_name(comm, nam_comm, &resultlen);
	if (comm == MPI_COMM_WORLD){
		sprintf(msg, " on comm %s\n", nam_comm);
		strcat(longmsg, msg);
	}else{
		local = find_comm_rank(nam_comm);
		sprintf(msg, " on comm %s %d\n", nam_comm, local);
		strcat(longmsg, msg);
	}


	bcount=bcount+2;

#ifdef WITH_MPI
	returnVal = PMPI_Bcast( buffer, count, datatype, root, comm );
#endif

#ifdef PAPI
	papi_get_end_measurement();
#endif
	return returnVal;
}

int   MPI_Reduce( sendbuf, recvbuf, count, datatype, op, root, comm )
const void * sendbuf;
void * recvbuf;
int count;
MPI_Datatype datatype;
MPI_Op op;
int root;
MPI_Comm comm;
{
	int   returnVal=0;
	int llrank, local;
	char msg[100];
	int np;
	char nam[MPI_MAX_OBJECT_NAME];
	char nam_comm[MPI_MAX_OBJECT_NAME];
	int resultlen;
#ifdef PAPI
	papi_get_start_measurement();
#endif
	PMPI_Type_get_name(datatype,nam,&np);
	np=encode_datatype((const char*)&nam);

	if (bcount>buff )
	{
		fprintf(fp, "%s", longmsg);
		longmsg[0]='\0';
		bcount=0;
	}

	PMPI_Comm_rank( MPI_COMM_WORLD, &llrank );
#ifdef PAPI
	papi_print_compute(msg, llrank);
	PAPI_accum_counters(values, 1);
	ins2=values[0];
#endif
	//sprintf(msg, "%d comm_size %d\n",llrank,global);//????????????

#ifdef WITH_MPI
	returnVal = PMPI_Reduce( sendbuf, recvbuf, count, datatype, op, root, comm );
#endif

	bcount=bcount+3;
#ifdef PAPI
	PAPI_accum_counters(values, 1);
	if(en_time==1) start_time=PAPI_get_real_usec();
	ins1=values[0];
#endif
	int size;
	PMPI_Type_size(datatype, &size); 
#ifdef PAPI
	sprintf(msg, "%d reduce %d (of %d bytes) %lld",llrank,count,size,ins1-ins2);
#else
	sprintf(msg, "%d reduce %d (of %d bytes) %d",llrank,count,size, root);
#endif
	strcat(longmsg, msg);
	
	sprintf(msg, " of type %d",np);
	strcat(longmsg, msg);
	
//	print_op(op);

	PMPI_Comm_get_name(comm, nam_comm, &resultlen);
	if (comm == MPI_COMM_WORLD){
		sprintf(msg, " on comm %s\n", nam_comm);
		strcat(longmsg, msg);
	}else{
		local = find_comm_rank(nam_comm);
		sprintf(msg, " on comm %s %d\n", nam_comm, local);
		strcat(longmsg, msg);
	}


#ifdef PAPI	//????? needed or get_end_measurement
	PAPI_accum_counters(values, 1);
	ins1=values[0];
#endif
	return returnVal;
}

int   MPI_Comm_rank( comm, rank )
MPI_Comm comm;
int * rank;
{
	int   returnVal=0;
	int llrank;
#ifdef PAPI
	papi_get_start_measurement();
#endif
	PMPI_Comm_rank( MPI_COMM_WORLD, &llrank );
#ifdef PAPI
	papi_print_compute(msg, llrank);
	strcat(longmsg,msg);
#endif
	returnVal = PMPI_Comm_rank( comm, rank );
#ifdef PAPI
	papi_get_end_measurement();
#endif
	return returnVal;
}

int   MPI_Comm_size( comm, size )
MPI_Comm comm;
int * size;
{
	int   returnVal=0;
	int llrank;
#ifdef PAPI
	papi_get_start_measurement();
#endif
	PMPI_Comm_rank( MPI_COMM_WORLD, &llrank );
#ifdef PAPI
	papi_print_compute(msg, llrank);
	strcat(longmsg,msg);
#endif
	returnVal = PMPI_Comm_size( comm, size );
	global=*size;
#ifdef PAPI
	papi_get_end_measurement();
#endif
	return returnVal;
}

int  MPI_Finalize(  )
{
	int  returnVal;
	int llrank;
#ifdef PAPI
	if(en_time==1) {
		end_time=PAPI_get_real_usec();
		printf("total time %lld\n",end_time-start_time); 
	}
	if(PAPI_accum_counters(values, 1)!=PAPI_OK) printf("This PAPI event is not supported\n");
#endif
	PMPI_Comm_rank( MPI_COMM_WORLD, &llrank );
#ifdef PAPI
	printf("%d total %lld\n",llrank,values[0]-t1);
#endif
	if (bcount>0 )
	{
		fprintf(fp, "%s", longmsg);
		longmsg[0]='\0';
		bcount=0;
	}
#ifdef PAPI
	if(en_time==1) fprintf(fp,"%d compute %lld %.6f\n",llrank,values[0]-ins1,(double)(end_time-start_time)/1000000);
	else  fprintf(fp,"%d compute %lld\n",llrank,values[0]-ins1);
#endif
	fprintf(fp,"%d finalize\n",llrank);
	fclose(fp);
#ifdef PAPI
	PAPI_stop_counters(values, 1);
#endif
	MINI_Trace_hasBeenFinished = 1;  
	returnVal = PMPI_Finalize(  );
	//deallocating resources
	delete_contig_list();
	delete_comm_list();
	delete_req_list();

	if (llrank == 0){
		printf("MINI ENDING!\n");
	}

	return returnVal;
}

int  MPI_Init( argc, argv )
int * argc;
char *** argv;
{
	int  returnVal;
	int llrank;
	char file[50];
	
	returnVal = PMPI_Init( argc, argv );
#ifdef PAPI
	int event_code;
	testt=getenv("MINI_TIME");
	en_time=atoi(testt);

	testt=getenv("MINI_METRIC");
	PAPI_library_init(PAPI_VER_CURRENT);
	if(PAPI_create_eventset(&EventSet)!=PAPI_OK) {
		printf("Could not create the EventSet");
	}
	PAPI_event_name_to_code(testt,&event_code);

	PAPI_add_event(EventSet, event_code);
	Events[0]=event_code;
#endif
	MINI_Trace_hasBeenInit = 1;


	PMPI_Comm_rank( MPI_COMM_WORLD, &llrank );
	if (llrank == 0){
		printf("MINI STARTING!\n");
	}

	if (stat("$PWD/ti_traces", &st) == -1) {
		mkdir("$PWD/ti_traces", 0700);
	}
	sprintf(file,"ti_traces/ti_trace%d.txt",llrank);
	fp=fopen(file, "wb");
	fprintf(fp,"%d init 1\n",llrank);
#ifdef PAPI
	if(PAPI_start_counters(Events,1)!=PAPI_OK) {
		printf("error1\n");
	}
	if(en_time==1) start_time=PAPI_get_real_usec();
	if(PAPI_accum_counters(values, 1)!=PAPI_OK) printf("This PAPI event is not supported\n");

	if(en_time==1) start_time=PAPI_get_real_usec();
	ins1=values[0];
	t1=values[0];
#endif
	//init comm list
	insert_comm("MPI_COMM_WORLD", llrank);

	return returnVal;
}

int  MPI_Init_thread( argc, argv,requir,provided )
int * argc;
char *** argv;
int requir;
int *provided;
{
	int  returnVal;
	int llrank;
	char file[50];
	char cwd[1024];
	returnVal = PMPI_Init( argc, argv );
#ifdef PAPI
	int event_code;
	testt=getenv("MINI_TIME");
	en_time=atoi(testt);

	testt=getenv("MINI_METRIC");
	PAPI_library_init(PAPI_VER_CURRENT);
	if(PAPI_create_eventset(&EventSet)!=PAPI_OK) {
		printf("Could not create the EventSet");
	}
	PAPI_event_name_to_code(testt,&event_code);

	PAPI_add_event(EventSet, event_code);

	Events[0]=event_code;
#endif
	MINI_Trace_hasBeenInit = 1;

	PMPI_Comm_rank( MPI_COMM_WORLD, &llrank );
	getcwd(cwd,1024);
	strcat(cwd,"/ti_traces");
	printf("\n\n%s\n\n",cwd);
	if (stat(cwd, &st) == -1) {
		mkdir(cwd, 0700);
	}  
	sprintf(file,"ti_traces/ti_trace%d.txt",llrank);
	fp=fopen(file, "wb");
	fprintf(fp,"%d init 1\n",llrank);
#ifdef PAPI
	if(PAPI_start_counters(Events,1)!=PAPI_OK) printf("Can not start the PAPI counter\n");

	if(PAPI_accum_counters(values, 1)!=PAPI_OK) printf("This PAPI event is not supported\n");

	if(en_time==1) start_time=PAPI_get_real_usec();
	ins1=values[0];
	t1=values[0];
#endif

	return returnVal;
}

int  MPI_Initialized( flag )
int * flag;
{
	int  returnVal;

	returnVal = PMPI_Initialized( flag );

	return returnVal;
}

#ifdef FOO
/* Don't trace the timer calls */
double  MPI_Wtick(  )
{
	double  returnVal;
	int llrank;

	returnVal = PMPI_Wtick(  );

	return returnVal;
}

double  MPI_Wtime(  )
{
	double  returnVal;
	int llrank;

	returnVal = PMPI_Wtime(  );

	return returnVal;
}
#endif

int  MPI_Irecv( buf, count, datatype, source, tag, comm, request )
void * buf;
int count;
MPI_Datatype datatype;
int source;
int tag;
MPI_Comm comm;
MPI_Request * request;
{
	int  returnVal=0;
	int  llrank;
	int np, size;
	char nam[MPI_MAX_OBJECT_NAME];
	char nam_comm[MPI_MAX_OBJECT_NAME];
	int resultlen;
	
	PMPI_Type_get_name(datatype,nam,&np);
#ifdef PAPI
	papi_get_start_measurement();
#endif

	np=encode_datatype((const char*)&nam);

	PMPI_Type_size(datatype, &size);

	PMPI_Comm_rank( MPI_COMM_WORLD, &llrank );
	if (bcount>buff )
	{
		fprintf(fp, "%s", longmsg);
		longmsg[0]='\0';
		bcount=0;
	}

	PMPI_Comm_get_name(comm, nam_comm, &resultlen);

#ifdef PAPI
	papi_print_compute(msg, llrank);
	strcat(longmsg,msg);
#endif
	bcount=bcount+2;
	glob_size=count;
	glob_np=np;

#ifdef WITH_MPI
	returnVal = PMPI_Irecv( buf, count, datatype, source, tag, comm, request );
#endif

	insert_req(request, count, size, np, nam_comm);
	i_mode=1;
	i_counter++;
#ifdef PAPI
	papi_get_end_measurement();
#endif
	return returnVal;
}

int  MPI_Isend( buf, count, datatype, dest, tag, comm, request )
const void * buf;
int count;
MPI_Datatype datatype;
int dest;
int tag;
MPI_Comm comm;
MPI_Request * request;
{
	int  returnVal=0;
	int  llrank;
	char msg[100];
	int np;
	char nam[MPI_MAX_OBJECT_NAME];
	char nam_comm[MPI_MAX_OBJECT_NAME];
	int resultlen;
#ifdef PAPI
	papi_get_start_measurement();
#endif
	PMPI_Type_get_name(datatype,nam,&np);
	np=encode_datatype((const char*)&nam);

	PMPI_Comm_rank( MPI_COMM_WORLD, &llrank );

	if (bcount>buff )
	{
		fprintf(fp, "%s", longmsg);
		longmsg[0]='\0';
		bcount=0;
	}
#ifdef PAPI
	papi_print_compute(msg, llrank);
	strcat(longmsg,msg);
#endif
	int size;
	PMPI_Type_size(datatype, &size);

	sprintf(msg, "%d Isend %d %d (of %d bytes) %d",llrank,dest,count,size,np);
	strcat(longmsg,msg);

	PMPI_Comm_get_name(comm, nam_comm, &resultlen);
	sprintf(msg, " on comm %s\n", nam_comm);
	strcat(longmsg,msg);

	bcount = bcount + 2;
	
#ifdef WITH_MPI
	returnVal = PMPI_Isend( buf, count, datatype, dest, tag, comm, request );
#endif


#ifdef PAPI
	papi_get_end_measurement();
#endif
	return returnVal;
}

int  MPI_Recv( buf, count, datatype, source, tag, comm, status )
void * buf;
int count;
MPI_Datatype datatype;
int source;
int tag;
MPI_Comm comm;
MPI_Status * status;
{
	int  returnVal=0;
	int  llrank;
	char msg[100];
	int np;
	char nam[MPI_MAX_OBJECT_NAME];
	char nam_comm[MPI_MAX_OBJECT_NAME];
	int resultlen;
	PMPI_Type_get_name(datatype,nam,&np);
#ifdef PAPI
	papi_get_start_measurement();
#endif
	np=encode_datatype((const char*)&nam);

#ifdef HAVE_MPI_STATUS_IGNORE
	MPI_Status    tmp_status;
	if (status == MPI_STATUS_IGNORE)
	status = &tmp_status;
#endif
	PMPI_Comm_rank( MPI_COMM_WORLD, &llrank );

	if (bcount>buff )
	{
		fprintf(fp, "%s", longmsg);
		longmsg[0]='\0';
		bcount=0;
	}
#ifdef PAPI
	papi_print_compute(msg, llrank);
	strcat(longmsg,msg);
#endif
	int size;
	PMPI_Type_size(datatype, &size);

	sprintf(msg, "%d recv %d %d (of %d bytes) %d", llrank,source,count,size, np);
	strcat(longmsg, msg);

	PMPI_Comm_get_name(comm, nam_comm, &resultlen);
	sprintf(msg, " on comm %s\n", nam_comm);
	strcat(longmsg,msg);
	
	bcount = bcount + 2;

#ifdef WITH_MPI	
	returnVal = PMPI_Recv( buf, count, datatype, source, tag, comm, status );
#endif

#ifdef PAPI
	papi_get_end_measurement();
#endif
	return returnVal;
}

int  MPI_Send( buf, count, datatype, dest, tag, comm )
const void * buf;
int count;
MPI_Datatype datatype;
int dest;
int tag;
MPI_Comm comm;
{
	int  returnVal=0;
	int  llrank;
	char msg[100],temp_buff[100];
	int np;
	char nam[MPI_MAX_OBJECT_NAME];
	char nam_comm[MPI_MAX_OBJECT_NAME];
	int resultlen;
	PMPI_Type_get_name(datatype,nam,&np);
#ifdef PAPI
	papi_get_start_measurement();
#endif
	np=encode_datatype((const char*)&nam);

	PMPI_Comm_rank( MPI_COMM_WORLD, &llrank );  

	if (bcount>buff )
	{
		fprintf(fp, "%s", longmsg);
		longmsg[0]='\0';
		bcount=0;
	}
	int size;
	PMPI_Type_size(datatype, &size);

	if(i_mode==0) {
#ifdef PAPI
		papi_print_compute(msg, llrank);
		strcat(longmsg,msg);
#endif

		sprintf(msg, "%d send %d %d (of %d bytes) %d", llrank,dest,count,size,np);
		strcat(longmsg,msg);

		PMPI_Comm_get_name(comm, nam_comm, &resultlen);
		sprintf(msg, " on comm %s\n", nam_comm);
		strcat(longmsg,msg);

	}
	else  {
#ifdef PAPI
		papi_print_compute(temp_buff, llrank);
		strcat(temp_long,temp_buff);
#endif
		sprintf(temp_buff, "%d send %d %d (of %d bytes) %d", llrank,dest,count,size,np);
		strcat(temp_long, temp_buff);

		PMPI_Comm_get_name(comm, nam_comm, &resultlen);
		sprintf(temp_buff, " on comm %s\n", nam_comm);	
		strcat(temp_long,temp_buff);
		temp_buff[0]='\0';
	}

#ifdef WITH_MPI
	returnVal = PMPI_Send( buf, count, datatype, dest, tag, comm );
#endif

	bcount=bcount+2;
#ifdef PAPI
	papi_get_end_measurement();
#endif
	return returnVal;
}

int   MPI_Wait( request, status )
MPI_Request * request;
MPI_Status * status;
{
	int   returnVal=0;
	int llrank;
	char msg[100];
#ifdef PAPI
	papi_get_start_measurement();
#endif
	PMPI_Comm_rank( MPI_COMM_WORLD, &llrank );

	if (bcount>buff )
	{
		fprintf(fp, "%s", longmsg);
		longmsg[0]='\0';
		bcount=0;
	}
	
	mpi_request_metadata *meta;
	if (i_mode == 1){
		meta = find_and_pop_req(request);
	}else{
		meta = NULL;
	}
#ifdef WITH_MPI
	returnVal = PMPI_Wait( request, status );
#endif

	if(i_mode==1) {
		if(meta != NULL){
			 sprintf(msg,"%d Irecv %d %d (of %d bytes) %d on comm %s\n",llrank,status->MPI_SOURCE, meta->count, meta->size, meta->data_code, meta->comm_name);
//			delete_req(request);
			free(meta);
		}else{
			 sprintf(msg,"%d Irecv %d\n",llrank,status->MPI_SOURCE);
		}
		strcat(longmsg,msg);
		i_counter--;
		if (i_counter==0){
			strcat(longmsg,temp_long);
		}
	}
#ifdef PAPI
	papi_print_compute(msg, llrank);
	strcat(longmsg,msg);
#endif
	sprintf(msg,"%d wait\n",llrank);
	strcat(longmsg,msg);
	if(i_counter==0){
		i_mode=0;
		temp_long[0]='\0';
	}

	bcount=bcount+3;
#ifdef PAPI
	papi_get_end_measurement();
#endif
	return returnVal;
}


//!!!!!!!!!!!!!!!!!!!!!!!MY ADDITIONS!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!//
int   MPI_Waitall( count, array_of_requests, array_of_statuses )
int count;
MPI_Request array_of_requests[];
MPI_Status *array_of_statuses;
{
	int i,bcounter=0;
	int   returnVal=0;
	int llrank;
	char msg[100];
#ifdef PAPI
	papi_get_start_measurement();
#endif
	PMPI_Comm_rank( MPI_COMM_WORLD, &llrank );

	if (bcount>buff )
	{
		fprintf(fp, "%s", longmsg);
		longmsg[0]='\0';
		bcount=0;
	}

	mpi_request_metadata *meta;
	mpi_request_metadata **meta_arr;
	if (i_mode==1){
		meta_arr = (mpi_request_metadata**) malloc(count*sizeof(mpi_request_metadata*));
		for (i=0; i< count;i++){
			meta_arr[i] = find_req(&array_of_requests[i]);
		}
	}

#ifdef WITH_MPI
	returnVal = PMPI_Waitall( count, array_of_requests, array_of_statuses );
#endif

	if(i_mode==1) {
		for (i=0;i<count;i++){
			if (llrank != array_of_statuses[i].MPI_SOURCE){//not an Isend
				meta = meta_arr[i];
				if(meta != NULL){
					sprintf(msg,"%d Irecv %d %d (of %d bytes) %d on comm %s\n",llrank,array_of_statuses[i].MPI_SOURCE, meta->count, meta->size, meta->data_code, meta->comm_name);
					delete_req(&array_of_requests[i]);
				}else{
					 sprintf(msg,"%d Irecv %d\n",llrank,array_of_statuses[i].MPI_SOURCE);
				}
				strcat(longmsg,msg);
				i_counter--;
				
				bcounter++;
			}
		}
		free(meta_arr);
		if (i_counter==0){
			strcat(longmsg,temp_long);
		}
	}	
#ifdef PAPI
	papi_print_compute(msg, llrank);
	strcat(longmsg,msg);
#endif
	sprintf(msg,"%d waitall\n",llrank);
	strcat(longmsg,msg);
	if (i_counter==0){
		i_mode=0;
		temp_long[0]='\0';
	}

		bcount=bcount+ bcounter + 1;
#ifdef PAPI
	papi_get_end_measurement();
#endif
	return returnVal;

}

int MPI_Comm_split(comm, color, key, newcomm)
MPI_Comm comm;
int color;
int key;
MPI_Comm *newcomm;
{
	int llrank, local;
	int returnVal;
	char msg[100];
	int size;
#ifdef PAPI
	papi_get_start_measurement();
#endif

	PMPI_Comm_rank( MPI_COMM_WORLD, &llrank );

	if (bcount>buff ){
		fprintf(fp, "%s", longmsg);
		longmsg[0]='\0';
		bcount=0;
	}
	returnVal = PMPI_Comm_split(comm, color, key, newcomm);
	if (color == MPI_UNDEFINED){
		sprintf(msg, "Comm split invoked by process but with color=MPI_UNDEFINED\n");
		strcat(longmsg, msg);
		bcount = bcount + 1;
	}
	else{
		sprintf(msg, "%d Comm split. color=%d, key=%d.", llrank, color, key);	
		strcat(longmsg,msg);
		char *temp = (char*) malloc(16*sizeof(char));
		create_new_comm_name(color, temp);

		PMPI_Comm_set_name(*newcomm, (const char*)temp);
		PMPI_Comm_size(*newcomm, &size);
		
		PMPI_Comm_rank(*newcomm, &local);

		insert_comm(temp, local);
		
		sprintf(msg, "New comm %s of size %d\n", temp, size);
		strcat(longmsg, msg);
		
		bcount = bcount + 2;
		free(temp);
	}
#ifdef PAPI
	papi_get_end_measurement();
#endif
	return returnVal;
}

int MPI_Comm_free(comm)
MPI_Comm *comm;
{
	int returnVal;
	char msg[100];
	char name[MPI_MAX_OBJECT_NAME];
	int resultlen;
#ifdef PAPI
	papi_get_start_measurement();
#endif
	if (bcount>buff ){
		fprintf(fp, "%s", longmsg);
		longmsg[0]='\0';
		bcount=0;
	}
	
	PMPI_Comm_get_name(*comm, name, &resultlen);
	
	sprintf(msg, "Comm %s free.\n", name);	
	strcat(longmsg,msg);
	
	//should also delete it from the list!!
	delete_comm(name);
	
	returnVal = PMPI_Comm_free(comm);
	

	bcount = bcount + 1;
#ifdef PAPI
	papi_get_end_measurement();
#endif
	return returnVal;
}

////!!!!!!!!!!!!!!!!!!INCOMPLETE!!!!!!!////////
int MPI_Comm_create(comm, group, newcomm)
MPI_Comm comm;
MPI_Group group;
MPI_Comm *newcomm;
{
	int returnVal;
	char msg[100];
#ifdef PAPI
	papi_get_start_measurement();
#endif
	if (bcount>buff ){
		fprintf(fp, "%s", longmsg);
		longmsg[0]='\0';
		bcount=0;
	}

	sprintf(msg, "Comm create.\n");	
	strcat(longmsg,msg);
	returnVal = PMPI_Comm_create(comm, group, newcomm);

	bcount = bcount + 1;
#ifdef PAPI
	papi_get_end_measurement();
#endif
	return returnVal;

}
////!!!!!!!!!!!!!!!!!!!!!!///////////////////////

int MPI_Type_contiguous(count, oldtype, newtype)
int count;
MPI_Datatype oldtype;
MPI_Datatype *newtype;{
	int returnVal;
	char nam[MPI_MAX_OBJECT_NAME];
	int np;
#ifdef PAPI	
	papi_get_start_measurement();
#endif
	returnVal = PMPI_Type_contiguous(count, oldtype, newtype);

	PMPI_Type_get_name(oldtype,nam,&np);
	//printf("after get name, nam=%s np=%d\n",nam, np);

	if (encode_datatype((const char*) &nam) <=100) {
		
		//printf("inside if, sending %s\n",nam);
		//MPI_Type_get_name(*newtype,nam,&np);
		const char *temp = create_new_contig_name();
		PMPI_Type_set_name(*newtype, temp);
		insert_contig((char*)temp);
	}
#ifdef PAPI	
	papi_get_end_measurement();
#endif
	return returnVal;

}

int MPI_Rsend(buf, count, datatype, dest, tag, comm)
const void *buf;
int count;
MPI_Datatype datatype;
int dest;
int tag;
MPI_Comm comm;{
	int  returnVal=0;
	int  llrank;
	char msg[100],temp_buff[100];
	int np;
	char nam[MPI_MAX_OBJECT_NAME];
	PMPI_Type_get_name(datatype,nam,&np);
	char nam_comm[MPI_MAX_OBJECT_NAME];
	int resultlen;

#ifdef PAPI
	papi_get_start_measurement();
#endif
	np=encode_datatype((const char*)&nam);

	PMPI_Comm_rank( MPI_COMM_WORLD, &llrank );  

	if (bcount>buff )
	{
		fprintf(fp, "%s", longmsg);
		longmsg[0]='\0';
		bcount=0;
	}
	int size;
	PMPI_Type_size(datatype, &size);

	if(i_mode==0) {
#ifdef PAPI
		papi_print_compute(msg, llrank);
		strcat(longmsg,msg);
#endif
		sprintf(msg, "%d Rsend %d %d (of %d bytes) %d", llrank,dest,count,size,np);
		strcat(longmsg,msg);

		PMPI_Comm_get_name(comm, nam_comm, &resultlen);
		sprintf(msg, " on comm %s\n", nam_comm);
		strcat(longmsg,msg);
	}
	else  {
#ifdef PAPI
		papi_print_compute(temp_buff, llrank);
		strcat(temp_long,temp_buff);
#endif
		sprintf(temp_buff, "%d Rsend %d %d (of %d bytes) %d", llrank,dest,count,size,np);
		strcat(temp_long, temp_buff);

		PMPI_Comm_get_name(comm, nam_comm, &resultlen);
		sprintf(temp_buff, " on comm %s\n", nam_comm);
		strcat(temp_long,temp_buff);

		temp_buff[0]='\0';
	}

#ifdef WITH_MPI
	returnVal = PMPI_Rsend( buf, count, datatype, dest, tag, comm );
#endif

	bcount=bcount+2;
#ifdef PAPI
	papi_get_end_measurement();
#endif
	return returnVal;

}

int MPI_Bsend(buf, count, datatype, dest, tag, comm)
const void *buf;
int count;
MPI_Datatype datatype;
int dest;
int tag;
MPI_Comm comm;{
	int  returnVal=0;
	int  llrank;
	char msg[100],temp_buff[100];
	int np;
	char nam[MPI_MAX_OBJECT_NAME];
	PMPI_Type_get_name(datatype,nam,&np);
	char nam_comm[MPI_MAX_OBJECT_NAME];
	int resultlen;

#ifdef PAPI
	papi_get_start_measurement();
#endif
	np=encode_datatype((const char*)&nam);

	PMPI_Comm_rank( MPI_COMM_WORLD, &llrank );  

	if (bcount>buff )
	{
		fprintf(fp, "%s", longmsg);
		longmsg[0]='\0';
		bcount=0;
	}
	int size;
	PMPI_Type_size(datatype, &size);

	if(i_mode==0) {
#ifdef PAPI
		papi_print_compute(msg, llrank);
		strcat(longmsg,msg);
#endif
		sprintf(msg, "%d Bsend %d %d (of %d bytes) %d", llrank,dest,count,size,np);
		strcat(longmsg,msg);

		PMPI_Comm_get_name(comm, nam_comm, &resultlen);
		sprintf(msg, " on comm %s\n", nam_comm);
		strcat(longmsg,msg);
	}
	else  {
#ifdef PAPI
		papi_print_compute(temp_buff, llrank);
		strcat(temp_long,temp_buff);
#endif
		sprintf(temp_buff, "%d Bsend %d %d (of %d bytes) %d", llrank,dest,count,size,np);
		strcat(temp_long,temp_buff);
		PMPI_Comm_get_name(comm, nam_comm, &resultlen);
		sprintf(temp_buff, " on comm %s\n", nam_comm);
		strcat(temp_long,temp_buff);

		temp_buff[0]='\0';
	}

#ifdef WITH_MPI
	returnVal = PMPI_Bsend( buf, count, datatype, dest, tag, comm );
#endif

	bcount=bcount+2;
#ifdef PAPI
	papi_get_end_measurement();
#endif
	return returnVal;

}

int MPI_Sendrecv(sendbuf, sendcount, sendtype, dest, sendtag, recvbuf, recvcount, recvtype, source, recvtag, comm, status)
const void *sendbuf;
int sendcount;
MPI_Datatype sendtype;
int dest;
int sendtag;
void *recvbuf;
int recvcount;
MPI_Datatype recvtype;
int source;
int recvtag;
MPI_Comm comm;
MPI_Status *status;{
	int  returnVal=0;
	int  llrank;
	char msg[100];
	int np, np2;
	char nam[MPI_MAX_OBJECT_NAME];
	char nam2[MPI_MAX_OBJECT_NAME];
	char nam_comm[MPI_MAX_OBJECT_NAME];
	int resultlen;

	PMPI_Type_get_name(sendtype,nam,&np);
	PMPI_Type_get_name(recvtype,nam2,&np2);
#ifdef PAPI
	papi_get_start_measurement();
#endif
	np=encode_datatype((const char*)&nam);
	np2=encode_datatype((const char*)&nam2);

	PMPI_Comm_rank( MPI_COMM_WORLD, &llrank );  

	if (bcount>buff )
	{
		fprintf(fp, "%s", longmsg);
		longmsg[0]='\0';
		bcount=0;
	}
	int ssize,rsize;
	PMPI_Type_size(sendtype, &ssize);
	PMPI_Type_size(recvtype, &rsize);

#ifdef PAPI
		papi_print_compute(msg, llrank);
		strcat(longmsg,msg);
#endif

	sprintf(msg, "%d Sendrecv(s) %d %d (of %d bytes) %d", llrank,dest,sendcount,ssize,np);
	strcat(longmsg,msg);

	PMPI_Comm_get_name(comm, nam_comm, &resultlen);
	sprintf(msg, " on comm %s\n", nam_comm);
	strcat(longmsg,msg);

	sprintf(msg, "%d Sendrecv(r) %d %d (of %d bytes) %d", llrank,source,recvcount,rsize,np2);
	strcat(longmsg,msg);

	sprintf(msg, " on comm %s\n", nam_comm);
	strcat(longmsg,msg);

#ifdef WITH_MPI
	returnVal = PMPI_Sendrecv(sendbuf, sendcount, sendtype, dest, sendtag, recvbuf, recvcount, recvtype, source, recvtag, comm, status);
#endif

	bcount=bcount+2;
#ifdef PAPI
	papi_get_end_measurement();
#endif
	return returnVal;


}

int MPI_Ssend(buf, count, datatype, dest, tag, comm)
const void *buf;
int count;
MPI_Datatype datatype;
int dest;
int tag;
MPI_Comm comm;{
	int  returnVal=0;
	int  llrank;
	char msg[100],temp_buff[100];
	int np;
	char nam[MPI_MAX_OBJECT_NAME];
	char nam_comm[MPI_MAX_OBJECT_NAME];
	int resultlen;

	PMPI_Type_get_name(datatype,nam,&np);
#ifdef PAPI
	papi_get_start_measurement();
#endif
	np=encode_datatype((const char*)&nam);

	PMPI_Comm_rank( MPI_COMM_WORLD, &llrank );  

	if (bcount>buff )
	{
		fprintf(fp, "%s", longmsg);
		longmsg[0]='\0';
		bcount=0;
	}
	int size;
	PMPI_Type_size(datatype, &size);

	if(i_mode==0) {
#ifdef PAPI
		papi_print_compute(msg, llrank);
		strcat(longmsg,msg);
#endif
		sprintf(msg, "%d Ssend %d %d (of %d bytes) %d", llrank,dest,count,size,np);
		strcat(longmsg,msg);

		PMPI_Comm_get_name(comm, nam_comm, &resultlen);
		sprintf(msg, " on comm %s\n", nam_comm);
		strcat(longmsg,msg);
	}
	else  {
#ifdef PAPI
		papi_print_compute(temp_buff, llrank);
		strcat(temp_long,temp_buff);
#endif
		sprintf(temp_buff, "%d Ssend %d %d (of %d bytes) %d", llrank,dest,count,size,np);
		strcat(temp_long,temp_buff);

		PMPI_Comm_get_name(comm, nam_comm, &resultlen);
		sprintf(temp_buff, " on comm %s\n", nam_comm);
		strcat(temp_long,temp_buff);

		temp_buff[0]='\0';
	}

#ifdef WITH_MPI
	returnVal = PMPI_Ssend( buf, count, datatype, dest, tag, comm );
#endif

	bcount=bcount+2;
#ifdef PAPI
	papi_get_end_measurement();
#endif
	return returnVal;

}

int MPI_Test(request, flag, status)
MPI_Request *request;
int *flag;
MPI_Status *status;{
	int returnVal;
	char msg[100];
#ifdef PAPI
	papi_get_start_measurement();
#endif
	if (bcount>buff ){
		fprintf(fp, "%s", longmsg);
		longmsg[0]='\0';
		bcount=0;
	}

	returnVal = PMPI_Test(request, flag, status);
	sprintf(msg, "Tested %d.\n", *flag);	
	strcat(longmsg,msg);
	
	bcount = bcount + 1;
#ifdef PAPI
	papi_get_end_measurement();
#endif
	return returnVal;


}

void mini_annotate_phase_start(char *name){

	char msg[100];
#ifdef PAPI
	papi_get_start_measurement();
#endif

	if (bcount>buff ){
		fprintf(fp, "%s", longmsg);
		longmsg[0]='\0';
		bcount=0;
	}

	sprintf(msg, "Phase %s start.\n", name);
	strcat(longmsg, msg);

	bcount=bcount+2;
#ifdef PAPI
	papi_get_end_measurement();
#endif
}

void mini_annotate_phase_end(char *name){

	char msg[100];
#ifdef PAPI
	papi_get_start_measurement();
#endif

	if (bcount>buff ){
		fprintf(fp, "%s", longmsg);
		longmsg[0]='\0';
		bcount=0;
	}

	sprintf(msg, "Phase %s end.\n", name);
	strcat(longmsg, msg);

	bcount=bcount+2;
#ifdef PAPI
	papi_get_end_measurement();
#endif
}

/**
* Compares two communicators. If they are identical (MPI_IDENT)
* returns 1. Otherwise returns 0.
**/
int compare_comms(MPI_Comm comm1, MPI_Comm comm2){
	int result;
	
	PMPI_Comm_compare(comm1, comm2, &result);
	
	if (result == MPI_IDENT) {
		return 1;
	}
	else {
		return 0;
	}
}

void print_op(MPI_Op op){
/*	char msg[50];
	switch (op){
	case MPI_MAX :
		sprintf(msg, ", op(max)");
		break;
	case MPI_MIN :
		sprintf(msg, ", op(min)");
		break;
	case MPI_SUM :
		sprintf(msg, ", op(sum)");
		break;
	case MPI_PROD :
		sprintf(msg, ", op(prod)");
		break;
	case MPI_LAND :
		sprintf(msg, ", op(land)");
		break;
	case MPI_BAND :
		sprintf(msg, ", op(band)");
		break;
	case MPI_LOR :
		sprintf(msg, ", op(lor)");
		break;
	case MPI_BOR :
		sprintf(msg, ", op(bor)");
		break;
	case MPI_LXOR :
		sprintf(msg, ", op(lxor)");
		break;
	case MPI_BXOR :
		sprintf(msg, ", op(bxor)");
		break;
	case MPI_MAXLOC :
		sprintf(msg, ", op(maxloc)");
		break;
	case MPI_MINLOC :
		sprintf(msg, ", op(minloc)");
		break;
	default:
		sprintf(msg, ", op(unknown)");
	}
	strcat(longmsg, msg);
*/
}
//!!!!!!!!!!!!!!!!!!!!!!!!!!!PAPI FUNCTIONS!!!!!!!!!!!!!!!!!!!!!!!!!!!
void papi_get_start_measurement(){
	if(en_time==1) end_time=PAPI_get_real_usec();
	if(PAPI_accum_counters(values, 1)!=PAPI_OK) printf("This PAPI event is not supported\n");
	ins2 = values[0];
}

void papi_get_end_measurement(){
	if(en_time==1) start_time=PAPI_get_real_usec();
	PAPI_accum_counters(values, 1);
	ins1=values[0];
}

void papi_print_compute(char msg[], int llrank){
	if(en_time==1) {
		sprintf(msg,"%d compute %lld %.6f\n",llrank,values[0]-ins1,(double)(end_time-start_time)/1000000);
	}
	else sprintf(msg,"%d compute %lld\n",llrank,values[0]-ins1);
}
//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

//!!!!!!!!!!!!!!!!!!!!!!!MY ADDITIONS!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!//
