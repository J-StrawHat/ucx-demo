# UCP Client Server Example

This example demonstrates a GPU version of the UCX "Hello World" script with a client and server instance. This example specifically includes the use of CUDA memory, Tag Matching API, Active Message API and Stream API.

## Compile this example
```bash
# First, edit the makefile to set the correct path of `UCX_PATH` and `CUDA_PATH`
make
```

## Run the example

### UCP Server Sample Output

(If the AM API is selected) The server gets a message from the client and if it is rendezvous request, initiates receive operation.

```bash
$ ./ucp_client_server -p 12345 -c am
server is listening on IP 0.0.0.0 port 12345
Waiting for connection...
Server received a connection request from client at address 192.168.0.211:38490
Server: iteration #1
UCX data message was received


----- UCP TEST SUCCESS -------

ABCDEFGHIJKLMNO.


------------------------------

sent FIN message
error handling callback was invoked with status -25 (Connection reset by remote peer)
Waiting for connection...
...
```

### UCP Client Sample Output

(If the AM API is selected) The client sends a message to the server and waits until the send is completed.

```bash
# Remember to point to the UCX libraries for the client terminal as well!
$ ./ucp_client_server -a 192.168.0.208 -p 12345 -m cuda -c am
Client: iteration #1


------------------------------

ABCDEFGHIJKLMNO.


------------------------------
```

## Reference

- [ucx / examples /ucp_client_server.c](https://github.com/openucx/ucx/blob/master/examples/ucp_client_server.c)
