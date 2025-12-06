#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>

#define EOF_CHAR '#'
#define MAX_BUFFER_SIZE 128

int receive_ack(int sock);
int send_ack(int sock);
void init();
struct sockaddr_in server_addr;

int print_help(char *args[]) {
	fprintf(stderr, "Type: %s -ip <serverIp> -p <serverPort>\n", args[0]);
	return 1;
}

int main(int num, char *args[])
{
	// Parse arguments
	if (num != 5)
	{
		return print_help(args);
	}
	if (strcmp(args[1], "-ip") != 0)
	{
		return print_help(args);
	}
	if (strcmp(args[3], "-p") != 0)
	{
		return print_help(args);
	}

	const char *serverIp = args[2];
	int serverPort = atoi(args[4]);

	// Create a socket
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == -1)
	{
		perror("Error data");
		return 1;
	}

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(serverPort);
	inet_pton(AF_INET, serverIp, &(server_addr.sin_addr));

	// Connect to server
	if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
	{
		perror("Error");
		return 1;
	}

	printf("Connected to server at %s:%d\n", serverIp, serverPort);

	while (1)
	{
		char command[4096];
		memset(command, 0, sizeof(command));
		printf("Enter a command for the server: ");

		// Read a user's command
		fgets(command, sizeof(command), stdin);
		
		char buffer[4096];
		memset(buffer, 0, sizeof(buffer));
		strcpy(buffer, command);

		char *ptr = strtok(command, " ");
		if (ptr != NULL && strcmp(ptr, "matinvpar") == 0)
		{
			ptr = strtok(NULL, " ");

			// parse command arguments
			int next = 0;
			while (ptr != NULL)
			{
				if (strcmp(ptr, "-n") == 0)
				{
					ptr = strtok(NULL, " ");
				}
				else if (strcmp(ptr, "-P") == 0)
				{
					ptr = strtok(NULL, " ");
				}
				else if (strcmp(ptr, "-m") == 0)
				{
					ptr = strtok(NULL, " ");
				}
				else if (strcmp(ptr, "-I") == 0)
				{
					ptr = strtok(NULL, " ");

					char* pbuf = (char *)malloc(strlen(ptr) + 1);
					if (pbuf != NULL)
					{
						strcpy(pbuf, ptr);
						size_t len = strcspn(pbuf, "\n");
						if (pbuf[len] == '\n')
						{
							pbuf[len] = '\0';
						}
					}

					if (strcmp(pbuf, "rand") != 0 && strcmp(pbuf, "fast") != 0)
					{
						printf("Invalid value for -I parameter. Please use 'rand' or 'fast'.\n");
						next = 1;
						break;
					}
				}
				else
				{
					printf("Invalid arguments!\nEnter:\n\tmatinvpar -n <size> -I <rand/fast> -P <0/1> -m <maxnum>\n\n");
					next = 1;
					break;
				}

				ptr = strtok(NULL, " ");
			}

			if (next == 1) {
				continue;
			}

			// send to server
			send(sock, buffer, strlen(buffer), 0);
		}
		else if (ptr != NULL && strcmp(ptr, "kmeanspar") == 0)
		{
			char *filePath = NULL;
			int numberOfClusters = -1;
			ptr = strtok(NULL, " ");

			int next = 0;
			while (ptr != NULL)
			{
				if (strcmp(ptr, "-f") == 0)
				{
					ptr = strtok(NULL, " ");
					filePath = ptr;
				}
				else if (strcmp(ptr, "-k") == 0)
				{
					ptr = strtok(NULL, " ");
					numberOfClusters = atoi(ptr);
				}

				else
				{
					printf("Invalid command!\nEnter:\n\tkmeanspar -f <data.txt> -k <cluster number>\n\n");
					next = 1;
					break;
				}

				ptr = strtok(NULL, " ");
			}
			if (next == 1) {
				continue;
			}

			if (numberOfClusters == -1 || filePath == NULL)
			{
				printf("Invalid command!\nEnter:\n\tkmeanspar -f <data.txt> -k <cluster number>\n\n");
				continue;
			}

			// send to server
			send(sock, buffer, strlen(buffer), 0);

			// receive ACK from server
			if (receive_ack(sock))
			{
				// open file to read data
				int readFd = open(filePath, O_RDONLY);
				if (readFd == -1)
				{
					perror("Error opening file");
					close(sock);
					return 1;
				}

				off_t offset = 0;
				struct stat fileInfo;

				// check if the file exists.
				if (fstat(readFd, &fileInfo) < 0)
				{
					perror("Error getting file information");
					close(readFd);
					close(sock);
					return 1;
				}

				// get file size
				off_t fileSize = fileInfo.st_size;

				while (fileSize > 0)
				{
					// send file data to server
					ssize_t data = sendfile(sock, readFd, &offset, MAX_BUFFER_SIZE);
					if (data == -1)
					{
						perror("Faild");
						close(readFd);
						close(sock);
						return 1;
					}

					fileSize -= data;
				}

				const char *valueEnd = "END_OF_DATA\n";
				send(sock, valueEnd, strlen(valueEnd), 0);

				close(readFd);
			}
			else
			{
				fprintf(stderr, "Failed to receive ACK from the server.\n");
			}
		}
		else
		{
			printf("Invalid command.\nEnter any:\n\tmatinvpar -n <size> -I <rand/fast> -P <0/1> -m <maxnum>.\n\tkmeanspar -f <data.txt> -k <cluster number>\n\n");
			continue;
		}

		char recvBuffer[MAX_BUFFER_SIZE];

		// Receiving the file from server
		ssize_t bytes_received = recv(sock, recvBuffer, sizeof(recvBuffer), 0);
		if (bytes_received == -1)
		{
			perror("Error receiving file name");
			close(sock);
			exit(EXIT_FAILURE);
		}
		//Set EOF
		recvBuffer[bytes_received] = '\0';

		//Send ACK to servber
		if (send_ack(sock))
		{
			char filePath[MAX_BUFFER_SIZE];
			snprintf(filePath, sizeof(filePath), "Client/results/%s", recvBuffer);

			//oepn file to write
			int writeFd = open(filePath, O_WRONLY | O_CREAT | O_TRUNC, 0666);
			if (writeFd == -1)
			{
				perror("Error creating file on client");
				close(sock);
				exit(EXIT_FAILURE);
			}

			char buf[MAX_BUFFER_SIZE];
			memset(buf, 0, sizeof(buf));
			ssize_t received_bytes = 0;

			// recv and write to file
			while ((received_bytes = recv(sock, buf, sizeof(buf), 0)) > 0)
			{
				if (received_bytes >= sizeof("\nEND_OF_DATA") - 1 &&
					strncmp(buf + received_bytes - sizeof("\nEND_OF_DATA") + 1, "\nEND_OF_DATA", sizeof("\nEND_OF_DATA") - 1) == 0)
				{
					write(writeFd, buf, received_bytes - sizeof("\nEND_OF_DATA") + 1);
					break;
				}

				write(writeFd, buf, received_bytes);
			}

			printf("Received the solution: %s.txt\n", recvBuffer);
			close(writeFd);
		}
	}

	close(sock);

	return 0;
}

int receive_ack(int sock)
{
	char ack[MAX_BUFFER_SIZE];
	ssize_t bytes_received = recv(sock, ack, sizeof(ack), 0);
	if (bytes_received <= 0)
	{
		perror("Error receiving ACK from server");
		return 0;
	}
	return 1;
}

int send_ack(int sock)
{
	char ack[] = "ACK";
	if (send(sock, ack, strlen(ack), 0) == -1)
	{
		perror("Error sending ACK to client");
		return 0;
	}
	return 1;
}