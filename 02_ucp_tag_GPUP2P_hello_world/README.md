# UCP GPU Hello World

This example demonstrates a GPU version of the UCX "Hello World" script with a client and server instance. This example specifically includes the use of CUDA memory, Tag Matching API.

## Compile this example
```
# First, edit the makefile to set the correct path of `UCX_PATH` and `CUDA_PATH`
make
```

## Run the example
```
# Start the server
./ucp_gpu_hello_world

# In a separate terminal window start the client 
./ucp_gpu_hello_world [server_name]
```

### UCP Server Sample Output

The server receives the address of the client and sends the message from CUDA Memory to the client's CUDA Memory.

```
$ ./ucp_gpu_hello_world 
...
UCX_LISTENER_BACKLOG=auto
UCX_PROTO_ENABLE=n
UCX_KEEPALIVE_INTERVAL=0.00us
UCX_KEEPALIVE_NUM_EPS=0
UCX_PROTO_INDIRECT_ID=auto
[1628286068.019413] [localmachine:25780:0]         parser.c:1689 UCX  WARN  unused env variable: UCX_ROOT (set UCX_WARN_UNUSED_ENV_VARS=n to suppress this warning)
[0xd3b91b80] local address length: 175
Waiting for connection...
[0xd3b91b80] receive handler called with status 0 (Success), length 183
UCX address message was received
flush_ep completed with status 0 (Success)
```

### UCP Client Sample Output

The client sends the local address to the server. Then the client receives the message (in the CUDA Memory) from the server and copy it to the host memory.

```
# Remember to point to the UCX libraries for the client terminal as well!
$ ./ucp_gpu_hello_world 192.168.0.208

...
UCX_FLUSH_WORKER_EPS=y
UCX_UNIFIED_MODE=n
UCX_SOCKADDR_CM_ENABLE=y
UCX_CM_USE_ALL_DEVICES=y
UCX_LISTENER_BACKLOG=auto
UCX_PROTO_ENABLE=n
UCX_KEEPALIVE_INTERVAL=0.00us
UCX_KEEPALIVE_NUM_EPS=0
UCX_PROTO_INDIRECT_ID=auto
[1628286097.787074] [localmachine:25841:0]         parser.c:1689 UCX  WARN  unused env variable: UCX_ROOT (set UCX_WARN_UNUSED_ENV_VARS=n to suppress this warning)
[0xd0d01740] local address length: 175
[0xd0d01740] receive handler called with status 0 (Success), length 24
UCX data message was received


----- UCP TEST SUCCESS ----

ABCDEFGHIJKLMNO

---------------------------
```

## Reference

- [ucx-tutorial-hot-interconnects / 02_ucx_gpu_example](https://github.com/gt-crnch-rg/ucx-tutorial-hot-interconnects/tree/main/examples/02_ucx_gpu_example)
