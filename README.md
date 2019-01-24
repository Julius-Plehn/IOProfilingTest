# IOProfilingTest

## Build
MPI Version:
```sh
$ make mpi
```
ScoreP Version:
```sh
$ make scorep
```

## Usage
```sh
$ ./writefile NumberOfElements Mode Iterations
```
**Mode 1:** Send data to single process which writes the data

**Mode 2:** All processes write their own data by using an offset

### Example
```sh
$ mpiexec -n 10 ./writefile 100000000 2 10
```
