#Set path to the base directory of your UCX installation
UCX_PATH=/home/zwx/lib/ucx-1.17.0/install
CUDA_PATH=/usr/local/cuda-11.7/

CC=nvcc
.c.o:
	$(CC) -L$(UCX_PATH)/lib -I$(UCX_PATH)/include -I$(CUDA_PATH) -lucp -lucs -luct -g -c -DHAVE_CUDA $<


ucp_client_server: ucp_client_server.o
	$(CC) $< -L$(UCX_PATH)/lib -I$(UCX_PATH)/include -lucp -lucs -luct -g -o $@

clean:
	rm -f *.o ucp_client_server *~
