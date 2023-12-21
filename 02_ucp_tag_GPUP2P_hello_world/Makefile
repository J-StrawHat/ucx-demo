#Set path to the base directory of your UCX installation
UCX_PATH=/home/oem/zwx/rdmalab/ucx-1.17.0/install/
CUDA_PATH=/usr/local/cuda-11.4/

CC=nvcc
.c.o:
	$(CC) -L$(UCX_PATH)/lib -I$(UCX_PATH)/include -I$(CUDA_PATH) -lucp -lucs -luct -g -c $<


ucp_hello_world: ucp_hello_world.o
	$(CC) $< -L$(UCX_PATH)/lib -I$(UCX_PATH)/include -lucp -lucs -luct -g -o $@

clean:
	rm *.o ucp_hello_world *~
