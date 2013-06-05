#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <time.h>
#include "md5.h"

typedef unsigned char byte;

int gen_traffic(int from,int to,int size, int count){
	int rank,i,namelen;
	char processor_name[MPI_MAX_PROCESSOR_NAME];
	byte*buffer;

	md5_state_t state;
	md5_byte_t digest[16];

	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Get_processor_name(processor_name, &namelen);
	if(from == rank){
		buffer = (char*)malloc(size*sizeof(byte));
		srand(time(NULL));
		for(i=0;i<size;i++) buffer[i] = rand()%256;

		md5_init(&state);
		md5_append(&state, (const md5_byte_t*)buffer, size*sizeof(byte));
		md5_finish(&state, digest);

		printf("(%2d)%s: send data (\"",rank,processor_name);
		for (i=0;i<32&&i<size;i++)printf("%02x",buffer[i]);
		printf("\")\n");
		printf("(%2d)%s: md5sum ",rank,processor_name);
		for (i=0;i<16;i++)
		    printf("%02x", digest[i]);
		printf("\n");

		MPI_Send(buffer,size*sizeof(byte),MPI_BYTE,to,0,MPI_COMM_WORLD);
		MPI_Send(&digest,16*sizeof(md5_byte_t),MPI_BYTE,to,0,MPI_COMM_WORLD);

		free(buffer);

	}else if(to == rank){
		buffer = (char*)malloc(size*sizeof(byte));
		MPI_Recv(buffer,size*sizeof(byte),MPI_BYTE,from,0,MPI_COMM_WORLD,0);
		MPI_Recv(digest,16*sizeof(md5_byte_t),MPI_BYTE,from,0,MPI_COMM_WORLD,0);

		printf("(%2d)%s: resv data (\"",rank,processor_name);
		for (i=0;i<32&&i<size;i++)printf("%02x",buffer[i]);
		printf("\")\n");

		printf("(%2d)%s: md5sum ",rank,processor_name);
		for (i=0;i<16;i++)
		    printf("%02x", digest[i]);
		printf("\n");

		md5_init(&state);
		md5_append(&state, (const md5_byte_t*)buffer, size*sizeof(byte));
		md5_finish(&state, digest);

		printf("(%2d)%s: md5sum ",rank,processor_name);
		for (i=0;i<16;i++)
		    printf("%02x", digest[i]);
		printf("\n");

		free(buffer);

	}
	return 0;
}

int main(int argc, char *argv[]) {
  int numprocs, rank, namelen;
  char processor_name[MPI_MAX_PROCESSOR_NAME];
  double curtime;



  MPI_Init(&argc, &argv);
  MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Get_processor_name(processor_name, &namelen);

  curtime = MPI_Wtime();

  printf("Process %d on %s out of %d (%lf)\n", rank, processor_name, numprocs, curtime);

  if(numprocs>=2)gen_traffic(0,1,1024*1024,1);

  MPI_Finalize();
}
