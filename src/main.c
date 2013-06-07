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
		printf("%.2lf %s",speed,s);
	}else if(abs(speed)<100){
		printf("%.1lf %s",speed,s);
	}else{
		printf("%.0lf %s",speed,s);
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

void printf_speed_d(double speed,double dspeed){
	printf_speed(speed);
	printf(" +/- ");
	printf_speed(dspeed);
}


int gen_traffic(int from,int to,size_t size, int count){
	int rank,iter,i,namelen,validation;
	char processor_name[MPI_MAX_PROCESSOR_NAME];
	byte*buffer;

	double tss,tsr,trs,trr,tsy,try,sending_time,speed,dspeed;
	double speed_sum;

	md5_state_t state;
	md5_byte_t digest[MD5_SIZE];
	md5_byte_t digest_control[MD5_SIZE];

	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Get_processor_name(processor_name, &namelen);
	if(from == rank){
		for(iter=0;iter<count;iter++){
			buffer = (char*)malloc(size*sizeof(byte));
			srand(time(NULL));
			for(i=0;i<size;i++) buffer[i] = rand()%256;

			md5_init(&state);
			md5_append(&state, (const md5_byte_t*)buffer, size*sizeof(byte));
			md5_finish(&state, digest);

	//		printf("(%2d)%s: send data (\"",rank,processor_name);
	//		for (i=0;i<32&&i<size;i++)printf("%02x",buffer[i]);
	//		printf("\")\n");
	//		printf("(%2d)%s: md5sum ",rank,processor_name);
	//		for (i=0;i<MD5_SIZE;i++)
	//		    printf("%02x", digest[i]);
	//		printf("\n");

			MPI_Barrier(MPI_COMM_WORLD);
			tsy = MPI_Wtime();
			MPI_Send(&tsy,1,MPI_DOUBLE,to,MPI_TAG_TIME,MPI_COMM_WORLD);

			tss = MPI_Wtime();
			MPI_Send(&tss,1,MPI_DOUBLE,to,MPI_TAG_TIME,MPI_COMM_WORLD);
			MPI_Send(buffer,size*sizeof(byte),MPI_BYTE,to,MPI_TAG_DATA,MPI_COMM_WORLD);
			MPI_Send(&digest,MD5_SIZE*sizeof(md5_byte_t),MPI_BYTE,to,MPI_TAG_HASH,MPI_COMM_WORLD);
			tsr = MPI_Wtime();
			MPI_Send(&tsr,1,MPI_DOUBLE,to,MPI_TAG_TIME,MPI_COMM_WORLD);

			free(buffer);
		}

	}else if(to == rank){
		speed_sum = 0;
		for(iter=0;iter<count;iter++){
			buffer = (char*)malloc(size*sizeof(byte));

			MPI_Barrier(MPI_COMM_WORLD);
			MPI_Recv(&tsy,1,MPI_DOUBLE,from,MPI_TAG_TIME,MPI_COMM_WORLD,0);
			try = MPI_Wtime();

			MPI_Recv(&tss,1,MPI_DOUBLE,from,MPI_TAG_TIME,MPI_COMM_WORLD,0);
			trs = MPI_Wtime();
			MPI_Recv(buffer,size*sizeof(byte),MPI_BYTE,from,MPI_TAG_DATA,MPI_COMM_WORLD,0);
			MPI_Recv(digest,MD5_SIZE*sizeof(md5_byte_t),MPI_BYTE,from,MPI_TAG_HASH,MPI_COMM_WORLD,0);
			trr = MPI_Wtime();
			MPI_Recv(&tsr,1,MPI_DOUBLE,from,MPI_TAG_TIME,MPI_COMM_WORLD,0);

			md5_init(&state);
			md5_append(&state, (const md5_byte_t*)buffer, size*sizeof(byte));
			md5_finish(&state, digest_control);

			validation = 1;
			for(i=0;i<MD5_SIZE;i++)if(digest[i]!=digest_control[i])validation = 0;


	//		printf("(%2d)%s: resv data (\"",rank,processor_name);
	//		for (i=0;i<32&&i<size;i++)printf("%02x",buffer[i]);
	//		printf("\")\n");

	//		printf("(%2d)%s: md5sum ",rank,processor_name);
	//		for (i=0;i<MD5_SIZE;i++)
	//		    printf("%02x", digest[i]);
	//		printf("\n");
	//
	//		printf("(%2d)%s: md5sum ",rank,processor_name);
	//		for (i=0;i<MD5_SIZE;i++)
	//		    printf("%02x", digest_control[i]);
	//		printf("\n");

			printf("Validation: ");
			if(validation){
				printf("OK\n");
			}else{
				printf("NONE\n");
			}

//			printf("Sender:\t sync %lf\tsend %lf\trecv %lf\tdiff %lf\n",tsy,tss,tsr,tsr-tss);
//			printf("Recver:\t sync %lf\tsend %lf\trecv %lf\tdiff %lf\n",try,trs,trr,trr-trs);
//			printf("Diffs :\t sync %lf\tsend %lf\trecv %lf\tdiff %lf\n",tsy-try,tss-trs,tsr-trr,tsr-tss-trr+trs);

			sending_time = trr - tss + tsy - try;

			speed = (double)(size+MD5_SIZE)/sending_time;
			dspeed = speed*1.e-3/sending_time;

			speed_sum += speed;
			printf("Speed: ");printf_speed(speed);printf("\n");
			printf("Avr Speed: ");printf_speed(speed_sum/(iter+1));printf("\n");

			free(buffer);
		}

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

  if(numprocs>=2)gen_traffic(0,1,1024*1024*24,10);

  MPI_Finalize();
}
