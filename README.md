# Concurrent-Math-Server-Matrix-Inversion-and-K-Means-over-Network-Sockets

A concurrent client–server system implemented in C, designed to compute two heavy mathematical workloads — matrix inversion (Gauss-Jordan elimination) and k-means clustering — for multiple clients over TCP.

The goal of the project is to simulate a real service: clients run on separate machines, send compute tasks to the server, and receive result files back over the network. The system handles concurrency using classic UNIX strategies (fork and multiplexing), while the computations themselves are parallelized using pthreads.

This repo follows the strict project structure required by the assignment.

⸻

Project Structure
project/
│
├── mathserver/
│   ├── include/
│   ├── objects/
│   ├── src/
│   │   ├── server.c
│   │   ├── matinv-par.c
│   │   ├── kmeans-par.c
│   │   └── ...
│   └── computed_results/
│
├── client/
│   ├── client.c
│   └── results/
│       ├── client1_results/
│       ├── client2_results/
│       └── ...
│
└── Makefile

Running make from the root directory builds both the server and client binaries and places them at the root level.

⸻

What the Server Does

The server listens on a TCP port, accepts multiple clients, and handles them using configurable strategies:

Supported Compute Tasks
	1.	Matrix Inversion (matinvpar)
Parallel Gauss-Jordan elimination using pthreads. Input parameters come from the client command, and results are saved to computed_results/ before being streamed back to the client.
	2.	k-Means Clustering (kmeanspar)
Parallel implementation of k-means using pthreads. The client sends the dataset file to the server, which computes cluster assignments and returns the results file.

Supported Concurrency Strategies
Strategy
Method
Grade Requirement
fork
One process per client
D / C / B / A
muxbasic
select() or poll()
B / A
muxscale
epoll() or kqueue()
A


The server never exits unless terminated manually (Ctrl+C). It must not leak zombie processes.

⸻

Server Command Line Options
./server -p <port> [-h] [-d] [-s strategy]
Options
	•	-p port — Port to listen on (required)
	•	-h — Print help
	•	-d — Run server as a daemon
	•	-s strategy — fork, muxbasic, or muxscale

If an unsupported option appears, the server prints help and exits with code 3.

⸻

Client Command Line Options
./client -ip <server_ip> -p <port>

After connecting, the client can send commands:
matinvpar -n 8 -I fast -P 1
kmeanspar -f kmeans-data.txt -k 9

Each result is received as a uniquely named file under client/results/clientX_results/.

⸻

System Workflow

From the server’s perspective:
$ ./server -p 9999
Listening for clients...
Connected with client 1
Client 1 commanded: matinvpar -n 8 -I fast -P 1
Sending solution: matinv_client1_soln1.txt
...

From the client’s perspective:
$ ./client -ip 127.0.0.1 -p 9999
Connected to server
Enter a command for the server: matinvpar -n 8 -I fast -P 1
Received the solution: matinv_client1_soln1.txt

Multiple clients can connect in parallel, send multiple sequential tasks, and receive multiple result files without overwriting earlier ones.

⸻

Parallelization Requirements

Both algorithms must use pthreads to parallelize the actual computation — not just initialization.
Matrix inverse and k-means programs must still support the same command-line flags as their sequential reference implementations.

⸻

What This Project Demonstrates
	•	Socket programming (TCP)
	•	Concurrent client handling using both process-based and multiplexed I/O strategies
	•	Multithreading with pthreads for compute tasks
	•	Safe file transfer over sockets
	•	Correct process management (no zombies)
	•	Real-world server behavior simulation

⸻

Build Instructions

From the project root:
make
This produces ./server and ./client.

⸻

Running the System

Start the server:
./server -p 9999 -s fork

Start one or more clients:
./client -ip 127.0.0.1 -p 9999

Enter tasks as text commands. Ctrl+C exits the client.

⸻

Author: Sai Sukheshwar Boganadula

Implemented as a full client–server system focusing on concurrency, parallel computation, and robust networking behavior. Built to behave like a real service, not a toy demo.
