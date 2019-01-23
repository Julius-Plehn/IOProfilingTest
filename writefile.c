#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <mpi.h>
#include <inttypes.h> 
#include <float.h>

//#define FILEPATH "/tmp/datafile"
#define FILEPATH "/mnt/lustre/plehn/testfile"

int rank, size;

void mode_1(int* items, int count){
	int* wholedata = NULL;
	if(rank == 0 && size > 1)
		wholedata = (int *) malloc(count * size * sizeof(int));
	
	int readback_rank = 0;
	if(size>1) {
		readback_rank = 1;
		// Gather data on rank 0
		MPI_Gather(items, count, MPI_INT, wholedata, count, MPI_INT, 0, MPI_COMM_WORLD);
	}

	if(rank == 0){
		int items_total = count * size;
		//printf("Total elements: %d\n", items_total);
		MPI_File fh;
		MPI_Status status;
		//int nbytes;
		// Open file
		MPI_File_open(MPI_COMM_SELF, FILEPATH, MPI_MODE_RDWR|MPI_MODE_CREATE, MPI_INFO_NULL, &fh);
		// Write data to file
		if(size>1){
			MPI_File_write(fh, wholedata, items_total, MPI_INT, &status);
		}else{
			MPI_File_write(fh, items, items_total, MPI_INT, &status);
		}
		//MPI_Get_count(&status, MPI_INT, &nbytes);
		/*
		MPI_Offset offset_size;
		MPI_File_get_size(fh, &offset_size);
		printf("File size is %" PRIdMAX " bytes.\n", (uintmax_t)offset_size);
		*/
		//printf("Written elements: %d\n", nbytes);
		MPI_File_close(&fh);
		if(size > 1){
			// Notify neigbour
			int notify = 1;
			//printf("Notify process 1...\n");
			MPI_Send(&notify, 1, MPI_INT, 1, 0, MPI_COMM_WORLD);
		}
	}
	free(wholedata);

	if(rank == readback_rank){
		int notify = 0;
		if(size > 1){
			//printf("Waiting for process 0...\n");
			MPI_Recv(&notify, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		}

		int items_total = count * size;
		MPI_File fh;
		MPI_Status status;
		
		// Open file
		MPI_File_open(MPI_COMM_SELF, FILEPATH, MPI_MODE_RDONLY|MPI_MODE_DELETE_ON_CLOSE, MPI_INFO_NULL, &fh);
		MPI_File_seek( fh, 0, MPI_SEEK_SET ); 

		int* readback_data = (int *) malloc(count * size * sizeof(int));
		MPI_File_read( fh, readback_data, items_total, MPI_INT, &status ); //_all

		// Validate file
	    int valid = 1;
	    for (int i=0; i<items_total; i++) {
	    	if(i != readback_data[i]){
	    		printf("File content is false!\n");
	    		valid = 0;
	    		break;
	        }
	    }
	    if(valid){
	    	printf("File verified!\n");
	    }
	    MPI_File_close(&fh);
	    free(readback_data);
	}
	
	//MPI_Barrier(MPI_COMM_WORLD);

}

void mode_2(int* items, int count){
	MPI_File fh;
	MPI_Status status;
	//int nbytes;
	
	// Offset
	MPI_Offset offset = count*rank;
	// Open file
	MPI_File_open(MPI_COMM_WORLD, FILEPATH, MPI_MODE_RDWR|MPI_MODE_CREATE, MPI_INFO_NULL, &fh);
	// Write data to file
	MPI_File_write_at(fh, offset*sizeof(int), items, count, MPI_INT, &status);
	// Ensure to write file to disk
	// TODO: Required???
	/*
	MPI_File_sync(fh);
	MPI_Barrier(MPI_COMM_WORLD);
	MPI_File_sync(fh);
	*/
	//MPI_Barrier(MPI_COMM_WORLD);
	MPI_File_close(&fh);

	MPI_File_open(MPI_COMM_WORLD, FILEPATH, MPI_MODE_RDONLY|MPI_MODE_DELETE_ON_CLOSE, MPI_INFO_NULL, &fh);

	if(rank == size-1){
		offset = 0;
	}else{
		offset += count;
	}
	// Read independent
	int* test = (int *) malloc(count*sizeof(int));
	MPI_File_read_at(fh, offset*sizeof(int), test, count, MPI_INT, &status);

	int valid = 1;

	for (int i=0; i<count; i++) {
		//printf("rank: %d | item: %d\n", rank, test[i]);
		if(i+offset != test[i]){
			printf("File content is false!\n");
			valid = 0;
			break;
	    }    
	}
	free(test);
	int globalvalid;
	MPI_Reduce(&valid, &globalvalid, 1, MPI_INT, MPI_MIN, 0,  MPI_COMM_WORLD);
	if(rank == 0 && globalvalid){
	    printf("File verified!\n");
	}

	MPI_Barrier(MPI_COMM_WORLD);
	MPI_File_close(&fh);
}

int main (int argc, char** argv)
{
	// MPI init
	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);
	// Timing
	double start, end, elapsed;
	if(argc < 4){
		if(rank == 0){
			printf("Missing argument(s)\n");
			printf("./writefile ELEMENTS MODE ITERATIONS\n");
			printf("MODE == 1: Send all elements to first process which performes the IO\n");
			printf("MODE == 2: Performe independent IO by each process with MPI-IO\n");
		}
		MPI_Finalize();
		return 0;
	}

	int items_each = atoi(argv[1])/size;
	int mode = atoi(argv[2]);
	int iterations = atoi(argv[3]);
	if(rank == 0){
		if(mode == 1){
			printf("\n--------------------------------------------------------\n");
			printf("Mode 1: Sending data to rank 0 which writes to a file...\n");
			printf("--------------------------------------------------------\n");
		}else if(mode == 2){
			printf("\n--------------------------------------------------------------\n");
			printf("Mode 2: Writing data with independent offset to single file...\n");
			printf("--------------------------------------------------------------\n");
		}
	}

	double* measurements = NULL;
	if(rank == 0)
		measurements = calloc(iterations, sizeof(double));
	int* items = (int *) malloc(items_each * sizeof(int));
	int relative_index = rank*items_each;
	for(int i = 0; i<items_each; ++i){
		items[i] = relative_index+i;
	}

	for(int i = 1; i<=iterations; ++i){
		MPI_Barrier(MPI_COMM_WORLD);
		if(rank == 0){
			if(i>1) printf("\n");
			printf("Iteration: %d/%d\n", i, iterations);
		}
		start = MPI_Wtime();
		if(mode == 1){
			mode_1(items, items_each);
		}else if(mode == 2){
			mode_2(items, items_each);
		}
		end = MPI_Wtime();
		elapsed = end-start;
		printf("%d: IO = %f\n", rank, elapsed);

		double globalelapsed;
		MPI_Reduce(&elapsed, &globalelapsed, 1, MPI_DOUBLE, MPI_MAX, 0,  MPI_COMM_WORLD);
		if(rank == 0){
			measurements[i-1] = globalelapsed;
		}
	}
	free(items);
	if(rank == 0){
		printf("\nMeasurements:\n");
		printf("-------------\n");
		double max = .0, mean = .0, sum = .0;
		double min = DBL_MAX;
		for(int i = 0; i<iterations; ++i){
			if(measurements[i] > max) max = measurements[i];
			if(measurements[i] < min) min = measurements[i];
			sum += measurements[i];
		}
		mean = sum/iterations;
		printf("Min: %f | Max: %f | Mean: %f\n", min, max, mean);
	}
	free(measurements);
	MPI_Finalize();
}