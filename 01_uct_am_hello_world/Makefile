#Set path to the base directory of your UCX installation
UCX_PATH=/home/oem/zwx/rdmalab/ucx-1.17.0/install/
CUDA_PATH=/usr/local/cuda-11.4/

CC=nvcc
.c.o:
	$(CC) -L$(UCX_PATH)/lib -I$(UCX_PATH)/include -I$(CUDA_PATH) -lucs -luct -g -c -DHAVE_CUDA $<


uct_hello_world: uct_hello_world.o
	$(CC) $< -L$(UCX_PATH)/lib -I$(UCX_PATH)/include -lucs -luct -g -o $@

clean:
	rm -f *.o uct_hello_world *~
