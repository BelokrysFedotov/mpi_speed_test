all: src/main.c src/md5.c src/md5.h
	mpicc src/md5.c src/main.c -o mpi_speed_test

clean:
	rm mpi_speed_test


