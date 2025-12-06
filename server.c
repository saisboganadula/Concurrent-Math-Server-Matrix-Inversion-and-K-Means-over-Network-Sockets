#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <math.h>
#include <stdbool.h>
#include <limits.h>
#include <pthread.h>
#include <sys/wait.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#define EOF_CHAR '#'
#define SERVER_RESULTS_DIR "mathserver/computed_results/"

#define MAX_BUFFER_SIZE 128
#define MAX_SIZE 4096
#define NUM_THREADSM 4

#define NUM_THREADS 8
#define MAX_POINTS 4096
#define MAX_CLUSTERS 32

int dp;
int serverSocket;
int k;

// MATRIX INVERSE PARALLELISED CODE

typedef double matrix[MAX_SIZE][MAX_SIZE];

int N;
int maxnum;
char *Init;
int PRINT;
matrix A;
matrix I;
pthread_barrier_t barrier;

void *parallelInverse(void *arg);
int sendAck(int sock);
int receiveAck(int sock);

void WriteMatrixFile(FILE *file, matrix M, char name[])
{
    fprintf(file, "%s Matrix:\n", name);
    for (int i = 0; i < N; i++)
    {
        for (int j = 0; j < N; j++)
        {
            fprintf(file, " %5.2f", M[i][j]);
        }
        fprintf(file, "\n");
    }
    fprintf(file, "\n\n");
}

void *parallelInverse(void *arg)
{
    int tid = (intptr_t)arg;

    int start = (N / NUM_THREADSM) * tid;
    int end = (tid == NUM_THREADSM - 1) ? N : start + (N / NUM_THREADSM);

    for (int z = 0; z < N; z++)
    {
        pthread_barrier_wait(&barrier);
        if (z >= start && z < end)
        {
            double value = A[z][z];

            for (int j = 0; j < N; j++)
            {
                A[z][j] = A[z][j] / value;
                I[z][j] = I[z][j] / value;
            }
            assert(A[z][z] == 1.0);
        }

        pthread_barrier_wait(&barrier);

        double multiplier = 0.0;
        for (int i = start; i < end; i++)
        {
            multiplier = A[i][z];
            if (i != z)
            {
                for (int j = 0; j < N; j++)
                {
                    A[i][j] = A[i][j] - A[z][j] * multiplier;
                    I[i][j] = I[i][j] - I[z][j] * multiplier;
                }
                assert(A[i][z] == 0.0);
            }
        }
    }

    return NULL;
}

typedef struct thread_args
{
    int start;
    int end;
    int k;
} thread_args;

typedef struct point
{
    float x;
    float y;
    int cluster;
} point;

point data[MAX_POINTS];
point cluster[MAX_CLUSTERS];

int gitCloseCenteroid(int i, int k)
{
    
    double dist, min_dist;
    min_dist = dist = INT_MAX;
    int nearestCluster = -1;
    for (int c = 0; c < k; c++)
    {
        double xx = data[i].x - cluster[c].x;
        double yy = data[i].y - cluster[c].y;
        dist = pow(xx,2) + pow(yy,2);
        if (dist <= min_dist)
        {
            min_dist = dist;
            nearestCluster = c;
        }
    }
    return nearestCluster;
}

void *assignClusterPointParallel(void *arg)
{
    thread_args *args = (thread_args *)arg;
    int first = args->start;
    int end = args->end;
    int m = args->k;

    bool someChanged = false;
    int oldCLuster = -1, nreCluster = -1;
    for (int i = first; i < end; i++)
    {
        oldCLuster = data[i].cluster;
        nreCluster = gitCloseCenteroid(i, m);
        data[i].cluster = nreCluster;
        if (oldCLuster != nreCluster)
        {
            someChanged = true;
        }
    }

    return (void *)someChanged;
}

int parseClusterNumter(char* sock)
{
    while (sock != NULL)
    {
        if (strcmp(sock, "-k") == 0)
        {
            sock = strtok(NULL, " ");
            return atoi(sock);
        }
        
        else if (strcmp(sock, "-f") == 0)
        {
            sock = strtok(NULL, " ");
        }
        sock = strtok(NULL, " ");
    }
    return 0;
}

int recvFileBuffer(int sock, int fd)
{
    int value = 0;
    while (1)
    {
        char size[MAX_BUFFER_SIZE];
        memset(size, 0, sizeof(size));

        ssize_t bytes = recv(sock, size, sizeof(size), 0);
        if (bytes <= 0)
        {
            if (bytes < 0)
            {
                perror("Error receiving data from client");
            }
            break;
        }

        write(fd, size, bytes);
        int i;
        for (i = 0; i < bytes; i++)
        {
            if (size[i] == '\n')
            {
                value++;
            }
        }

        if (strstr(size, "END_OF_DATA") != NULL)
        {
            break;
        }
    }
    return value;
}

// A FUNCTION TO HANDLE THE CLIENT'S REQUEST

void cleanUp()
{
    kill(0, SIGKILL);
    close(serverSocket);
    exit(0);
}

void handleClient(int client_socket, int current_client)
{
    char userInput[MAX_BUFFER_SIZE];
    ssize_t bytes_received = 0;
    int kmeansparCnt = 1;
    int matinvparCnt = 1;
    int fileNumter = 1;
    char resultPath[MAX_BUFFER_SIZE];
    char resultName[MAX_BUFFER_SIZE];
    
    while ((bytes_received = recv(client_socket, userInput, sizeof(userInput), 0)) > 0)
    {
        userInput[bytes_received] = '\0';
        printf("Client %d commanded : %s", current_client, userInput);

        char resultmName[MAX_BUFFER_SIZE];
        char *sock = strtok(userInput, " ");

        if (strcmp(sock, "matinvpar") == 0){
            snprintf(resultmName, sizeof(resultmName), "mathserver/computed_results/client%d_matinvpar%d_result", current_client, matinvparCnt);
            snprintf(resultName, sizeof(resultName), "client%d_matinvpar%d_result", current_client, matinvparCnt);
            matinvparCnt++; // Increment the matinvpar counter
            strncpy(resultPath, resultmName, sizeof(resultPath));
            // Default values
            N = 5;
            maxnum = 15;
            Init = malloc(strlen("fast") + 1);
            PRINT = 1;
            if (Init != NULL)
            {
                strcpy(Init, "fast");
            }
            while (sock != NULL)
            {
                if (strcmp(sock, "-P") == 0)
                {
                    sock = strtok(NULL, " ");
                    PRINT = atoi(sock);
                }
                
                else if (strcmp(sock, "-n") == 0)
                {
                    sock = strtok(NULL, " ");
                    N = atoi(sock);
                }

                
                else if (strcmp(sock, "-I") == 0)
                {
                    sock = strtok(NULL, " ");
                    Init = (char *)malloc(strlen(sock) + 1);
                    if (Init != NULL)
                    {
                        strcpy(Init, sock);
                        size_t len = strcspn(Init, "\n");
                        if (Init[len] == '\n')
                        {
                            Init[len] = '\0';
                        }
                    }
                }
                else if (strcmp(sock, "-m") == 0)
                {
                    sock = strtok(NULL, " ");
                    maxnum = atoi(sock);
                }
                sock = strtok(NULL, " ");
            }

            // initMatrix
            for (int i = 0; i < N; i++) {
                for (int j = 0; j < N; j++) {
                    if (i == j)
                        I[i][j] = 1.0;
                    else
                        I[i][j] = 0.0;  
                }
            }

            if (strcmp(Init, "fast") == 0)
            {   
                for (int i = 0; i < N; i++)
                {
                    for (int j = 0; j < N; j++)
                    {
                        if (i == j)
                            A[i][j] = 5.0;
                        else
                            A[i][j] = 2.0;
                    }
                }
            }
            if (strcmp(Init, "rand") == 0)
            {
                for (int i = 0; i < N; i++)
                {
                    for (int j = 0; j < N; j++)
                    {
                        if (i == j)
                            A[i][j] = (double)(rand() % maxnum) + 5.0;
                        else
                            A[i][j] = (double)(rand() % maxnum) + 1.0;
                    }
                }
            }


            pthread_barrier_init(&barrier, NULL, NUM_THREADSM);

            pthread_t threads[NUM_THREADSM];
            int i;
            for (i = 0; i < NUM_THREADSM; ++i)
            {
                if (pthread_create(&threads[i], NULL, parallelInverse, (void *)(intptr_t)i) != 0)
                {
                    fprintf(stderr, "Error creating thread %d\n", i);
                    exit(EXIT_FAILURE);
                }
            }

            for (int i = 0; i < NUM_THREADSM; ++i)
            {
                if (pthread_join(threads[i], NULL) != 0)
                {
                    fprintf(stderr, "Error joining thread %d\n", i);
                    exit(EXIT_FAILURE);
                }
            }

            pthread_barrier_destroy(&barrier);
            FILE *outputFile;
            outputFile = fopen(resultmName, "w");
            WriteMatrixFile(outputFile, A, "Initialized");
            WriteMatrixFile(outputFile, I, "Inversed");
            fprintf(outputFile, "\n\n"); // Add 2 lines space
            fclose(outputFile); // Close the file
            free(Init);
        }
        else if (strcmp(userInput, "kmeanspar") == 0)
        {
            char resultFileName[MAX_BUFFER_SIZE];
            snprintf(resultFileName, sizeof(resultFileName), "mathserver/computed_results/client%d_kmeanspar_result%d", current_client, kmeansparCnt);
            snprintf(resultName, sizeof(resultName), "client%d_kmeanspar_result%d", current_client, kmeansparCnt);
            strncpy(resultPath, resultFileName, sizeof(resultPath));
            kmeansparCnt++;
            char *resultName = basename(resultPath);
            char fileName[MAX_BUFFER_SIZE];
            snprintf(fileName, sizeof(fileName), "mathserver/recv_files/recdata%d.txt", fileNumter);

            int numberCluster = parseClusterNumter(sock);
            if (sendAck(client_socket))
            {
                int fileOpen = open(fileName, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                if (fileOpen == -1)
                {
                    perror("Error opening received file for writing");
                    close(client_socket);
                    return;
                }

                int lineCnt = recvFileBuffer(client_socket, fileOpen);
                dp = lineCnt - 1;

                close(fileOpen);

                //readData
               
                FILE *fOpen = fopen(fileName, "r");
                if (fOpen == NULL)
                {
                    perror("Cannot open the file");
                    exit(EXIT_FAILURE);
                }

                for ( int i = 0; i < dp; i++)
                {
                    if (fscanf(fOpen, "%f %f", &data[i].x, &data[i].y) != 2)
                    {
                        fprintf(stderr, "Error reading data point %d\n", i);
                        exit(EXIT_FAILURE);
                    }
                    data[i].cluster = -1;
                }
                // printf("Read the problem data!\n");

                srand(0);
                k = numberCluster;
                for (int i = 0; i < k; i++)
                {
                    int r = rand() % dp;
                    cluster[i].x = data[r].x;
                    cluster[i].y = data[r].y;
                }

                 fclose(fOpen);

                //kmeans
                bool somechange;
                int iter = 0;
                do
                {
                    iter++;
                    somechange = false;

                    pthread_t threads[NUM_THREADS];
                    thread_args threadArgs[NUM_THREADS];
                    int chunk_size = dp / NUM_THREADS;
                    int start = 0;

                    for (int i = 0; i < NUM_THREADS; i++)
                    {
                        threadArgs[i].start = start;
                        threadArgs[i].end = (i == NUM_THREADS - 1) ? dp : start + chunk_size;
                        threadArgs[i].k = numberCluster;

                        pthread_create(&threads[i], NULL, assignClusterPointParallel, (void *)&threadArgs[i]);

                        start += chunk_size;
                    }

                    for (int i = 0; i < NUM_THREADS; i++)
                    {
                        void *result;
                        pthread_join(threads[i], &result);
                        somechange = somechange || (bool)result;
                    }

                    // updateClusterCenter
                    int cnt[MAX_CLUSTERS] = {0};
                    point temp[MAX_CLUSTERS] = {0.0};
                    int a;
                    for (int i = 0; i < dp; i++)
                    {
                        a = data[i].cluster;
                        cnt[a]++;
                        temp[a].x += data[i].x;
                        temp[a].y += data[i].y;
                    }
                    for (int i = 0; i < numberCluster; i++)
                    {
                        cluster[i].x = temp[i].x / cnt[i];
                        cluster[i].y = temp[i].y / cnt[i];
                    }


                } while (somechange);

                FILE *file = fopen(resultFileName, "w");
                if (file == NULL)
                {
                    perror("Cannot open the file");
                    exit(EXIT_FAILURE);
                }

                for (int i = 0; i < dp; i++)
                {
                    fprintf(file, "%.2f %.2f %d\n", data[i].x, data[i].y, data[i].cluster);
                }
                fclose(file);



                fileNumter++;
                dp = 0;
            }
            else
            {
                fprintf(stderr, "Failed to send ACK to the client.\n");
            }
        }
        else
        {
            printf("Unknown command: %s\n", userInput);
        }

        printf("Sending solution: %s.txt\n", resultName);

        // Sending file back to the client
        send(client_socket, resultName, strlen(resultName), 0);
        if (receiveAck(client_socket))
        {
            int fileOpen2 = open(resultPath, O_RDONLY);
            if (fileOpen2 == -1)
            {
                perror("Error opening file");
                close(client_socket);
                return 1;
            }
            off_t offset = 0;
            struct stat file_info;
            if (fstat(fileOpen2, &file_info) < 0)
            {
                perror("Error getting file information");
                close(fileOpen2);
                close(client_socket);
                return 1;
            }

            off_t total_bytes_to_send = file_info.st_size;
            while (total_bytes_to_send > 0)
            {
                ssize_t sent_bytes = sendfile(client_socket, fileOpen2, &offset, MAX_BUFFER_SIZE);
                if (sent_bytes == -1)
                {
                    perror("Error sending file");
                    close(fileOpen2);
                    close(client_socket);
                    return 1;
                }

                total_bytes_to_send -= sent_bytes;
            }

            const char *end_of_data = "\nEND_OF_DATA";
            send(client_socket, end_of_data, strlen(end_of_data), 0);
            close(fileOpen2);
        }
    }

    close(client_socket);
    exit(0);
}

int printHelp(char *argv[])
{
    printf("Usage: %s -p <server_port> -h -d\n", argv[0]);
    printf("-p <server_port>  : Specify the server port\n");
    printf("-d                : Run as a daemon\n");
    printf("-h                : Display help\n");
}


int parseOption(int argc, char *argv[], bool *daemonMode)
{

    int option = 0;
    int serverPort = 0;
    while ((option = getopt(argc, argv, "p:hd")) != -1)
    {
        switch (option)
        {
        case 'd':
          *daemonMode = true;
        break;
        case 'p':
          serverPort = atoi(optarg);
        break;
        case 'h':
            printHelp(argv);
            return 0;   
        default:
            fprintf(stderr, "Usage: %s -p <server_port> -h -d \n", argv[0]);
            return 0;
        }
    }
    return serverPort;
}

void close_handles() {
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}

void open_dev_null() {
    open("/dev/null", O_RDONLY);
    open("/dev/null", O_WRONLY);
    open("/dev/null", O_WRONLY);
}

void startServer(int serverPort) {
    if (serverSocket == -1)
    {
        perror("Error creating socket");
        return 1;
    }

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    // Bind
    if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1)
    {
        perror("Error binding");
        return 1;
    }

    if (listen(serverSocket, 5) == -1)
    {
        perror("Error listening");
        return 1;
    }

    printf("Listening to clients on port %d\n", serverPort);

    //serverWork
    while (1)
    {
        int numberClients = 1;
        struct sockaddr_in client_addr;
        socklen_t client_size = sizeof(client_addr);
        int clientSocket = accept(serverSocket, (struct sockaddr *) &client_addr, &client_size);
        pid_t childpid = fork();  
        
        if (childpid == 0)
        {
            printf("Client %d connected from %s:%d\n", numberClients, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            // Handle client
            handleClient(clientSocket, numberClients);
        }
        if (clientSocket == -1)
        {
            perror("Error accepting connection");
            return 1;
        } 
        if (childpid < 0)
        {
            perror("Fork failed");
            return 1;
        }
        else
        {
            close(clientSocket);
            numberClients++;

            int status = 0;
            pid_t terminatedChildPID;
            while ((terminatedChildPID = waitpid(-1, &status, WNOHANG)) > 0)
            {
                printf("Child process %d terminated.\n", terminatedChildPID);
            }
        }
    }



    close(serverSocket);
}

int main(int argc, char *argv[])
{
    // char base_directory[MAX_BUFFER_SIZE];
    bool daemonMode = false;
    int serverPort = parseOption(argc, argv, &daemonMode);
    if (serverPort == 0) {
        return 1;
    }

    signal(SIGINT, cleanUp);

    void daemon()
    {
        umask(0);

        int pid = fork();
        if (pid < 0)
        {
            perror("Fork failed");
            return 1;
        }
        if (pid > 0)
        {
            exit(0);
        }

        setsid();

        close_handles();
        open_dev_null();
    }

    if (daemonMode == true)
    {
        daemon();
    }

    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    startServer(serverPort);
    return 0;
}

int sendAck(int sock)
{
    char buffer[] = "ACK";
    if (send(sock, buffer, strlen(buffer), 0) == -1)
    {
        perror("Error sending ACK to client");
        return 0;
    }
    return 1;
}

int receiveAck(int sock)
{
    char buffer[MAX_BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));

    ssize_t received = recv(sock, buffer, sizeof(buffer), 0);
    if (received <= 0)
    {
        perror("Error receiving ACK from server");
        return 0;
    }
    return 1;
}
