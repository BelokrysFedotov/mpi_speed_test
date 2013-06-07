#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <sys/types.h>
#include "md5.h"

typedef unsigned char byte;

#define MD5_SIZE 16

#define MPI_TAG_TIME 1
#define MPI_TAG_DATA 2
#define MPI_TAG_HASH 3

void printf_speed_(double speed,char*s){
	if(abs(speed)<10){
		printf("%.2lf%s",speed,s);
	}else if(abs(speed)<100){
		printf("%.1lf%s",speed,s);
	}else{
		printf("%.0lf%s",speed,s);
	}
}
void printf_speed(double speed){
	if(abs(speed)<1024){printf_speed_(speed,"B/s");return;}
	speed/=1024;
	if(abs(speed)<1024){printf_speed_(speed,"KB/s");return;}
	speed/=1024;
	if(abs(speed)<1024){printf_speed_(speed,"MB/s");return;}
	speed/=1024;
	if(abs(speed)<1024){printf_speed_(speed,"GB/s");return;}
	speed/=1024;
	if(abs(speed)<1024){printf_speed_(speed,"TB/s");return;}
}

void printf_size(double size){
	if(abs(size)<1024){printf_speed_(size,"B");return;}
	size/=1024;
	if(abs(size)<1024){printf_speed_(size,"KB");return;}
	size/=1024;
	if(abs(size)<1024){printf_speed_(size,"MB");return;}
	size/=1024;
	if(abs(size)<1024){printf_speed_(size,"GB");return;}
	size/=1024;
	if(abs(size)<1024){printf_speed_(size,"TB");return;}
}

void printf_counts(double counts){
	if(abs(counts)<1000){printf("%.0lf",counts);return;}
	counts/=1000;
	if(abs(counts)<1000){printf_speed_(counts,"K");return;}
	counts/=1000;
	if(abs(counts)<1000){printf_speed_(counts,"M");return;}
	counts/=1000;
	if(abs(counts)<1000){printf_speed_(counts,"G");return;}
	counts/=1000;
	if(abs(counts)<1000){printf_speed_(counts,"T");return;}
}

int gen_traffic(int from,int to,int size, int count){
	int rank,iter,i,namelen,validation;
	char name[MPI_MAX_PROCESSOR_NAME];
	byte*buffer;

	double tss,tsr,trs,trr,tsy,try,sending_time,speed;
	double speed_sum;

	MPI_Status status;

	md5_state_t state;
	md5_byte_t digest[MD5_SIZE];
	md5_byte_t digest_control[MD5_SIZE];

	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Get_processor_name(name, &namelen);
	if(from == rank){
		for(iter=0;iter<count;iter++){
			buffer = (char*)malloc(size*sizeof(byte));
			srand(time(NULL));
			for(i=0;i<size;i++) buffer[i] = rand()%256;

			md5_init(&state);
			md5_append(&state, (const md5_byte_t*)buffer, size*sizeof(byte));
			md5_finish(&state, digest);

			MPI_Barrier(MPI_COMM_WORLD);
			tsy = MPI_Wtime();
			MPI_Send(&tsy,1,MPI_DOUBLE,to,MPI_TAG_TIME,MPI_COMM_WORLD);

			tss = MPI_Wtime();
			MPI_Send(&tss,1,MPI_DOUBLE,to,MPI_TAG_TIME,MPI_COMM_WORLD);
			MPI_Send(buffer,size*sizeof(byte),MPI_BYTE,to,MPI_TAG_DATA,MPI_COMM_WORLD);
			MPI_Send(digest,MD5_SIZE*sizeof(md5_byte_t),MPI_BYTE,to,MPI_TAG_HASH,MPI_COMM_WORLD);
			tsr = MPI_Wtime();
			MPI_Send(&tsr,1,MPI_DOUBLE,to,MPI_TAG_TIME,MPI_COMM_WORLD);

			free(buffer);
		}

	}else if(to == rank){
		speed_sum = 0;
		for(iter=0;iter<count;iter++){
			buffer = (char*)malloc(size*sizeof(byte));

			MPI_Barrier(MPI_COMM_WORLD);
			MPI_Recv(&tsy,1,MPI_DOUBLE,from,MPI_TAG_TIME,MPI_COMM_WORLD,&status);
			try = MPI_Wtime();

			MPI_Recv(&tss,1,MPI_DOUBLE,from,MPI_TAG_TIME,MPI_COMM_WORLD,&status);
			trs = MPI_Wtime();
			MPI_Recv(buffer,size*sizeof(byte),MPI_BYTE,from,MPI_TAG_DATA,MPI_COMM_WORLD,&status);
			MPI_Recv(digest,MD5_SIZE*sizeof(md5_byte_t),MPI_BYTE,from,MPI_TAG_HASH,MPI_COMM_WORLD,&status);
			trr = MPI_Wtime();
			MPI_Recv(&tsr,1,MPI_DOUBLE,from,MPI_TAG_TIME,MPI_COMM_WORLD,&status);

			md5_init(&state);
			md5_append(&state, (const md5_byte_t*)buffer, size*sizeof(byte));
			md5_finish(&state, digest_control);

			validation = 1;
			for(i=0;i<MD5_SIZE;i++)if(digest[i]!=digest_control[i])validation = 0;

			if(!validation){
				printf("Validation error\n");
				exit(2);
			}

			sending_time = trr - tss + tsy - try;

			speed = (double)(size+MD5_SIZE)/sending_time;

			speed_sum += speed;

			free(buffer);
		}

		printf("avr speed: ");printf_speed(speed_sum/iter);printf("\t");

	}else{
		for(iter=0;iter<count;iter++){
			MPI_Barrier(MPI_COMM_WORLD);
		}
	}



	return 0;
}

int main(int argc, char *argv[]) {

	int numprocs, rank, namelen;
	char name[MPI_MAX_PROCESSOR_NAME];

	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	MPI_Status status;

	if(argc<5){
		if(rank==0)printf("Run programs with parameters: `program %%number_from%% %%number_to%% %%size_in_kb%% %%count_of_iterations%%`\n");
		return 1;
	}

	int rank_from,rank_to,size,count;
	rank_from	= atoi(argv[1]);
	rank_to		= atoi(argv[2]);
	size		= atoi(argv[3]);
	count		= atoi(argv[4]);

	if(rank_from>=numprocs || rank_to>=numprocs){
		if(rank==0)printf("Ranks out of count of procs\n");
		return 1;
	}

	if(rank==rank_from){
		MPI_Get_processor_name(name, &namelen);
		MPI_Send(name,MPI_MAX_PROCESSOR_NAME,MPI_CHAR,rank_to,0,MPI_COMM_WORLD);
	}else if(rank==rank_to){
		MPI_Recv(name,MPI_MAX_PROCESSOR_NAME,MPI_CHAR,rank_from,0,MPI_COMM_WORLD,&status);
		printf("form %s [%d]\t",name,rank_from);
	}

	MPI_Barrier(MPI_COMM_WORLD);
	if(rank==rank_to){
		MPI_Get_processor_name(name, &namelen);
		printf("to %s [%d]\t",name,rank_to);
	}
	MPI_Barrier(MPI_COMM_WORLD);

	if(rank==rank_to){
		printf("size ");printf_size(size);printf("\t");
		printf("count ");printf_counts(count);printf("\t");
	}
	MPI_Barrier(MPI_COMM_WORLD);

	gen_traffic(rank_from,rank_to,size*1024,count);

	if(rank==rank_to)printf("\n");

	MPI_Finalize();
	return 0;
}
