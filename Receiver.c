#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h> 
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h> 

#define SERVER_PORT 5060

// How many measures can we save (the Ex rquired 5)
#define SIZE 10

// Authentication
#define AUTHENTICATION "ack"

/*
*  Prints:
*  (1) The time
*  (2) The average time for each part of the received files
*  (3) The average file
*/
void printBeforeExit(double time[2][SIZE], uint size)
{
	if (size <= 0)
	{
		printf("\n**No data received at all**\n");
		return;
	}
    double sum1 = 0, sum2 = 0, avg1, avg2;
    printf("\nPrinting times:\n");
    for (int i = 0; i < 2; i++)
    {
        printf("\nCC number %d:\n", (i + 1));
        for (int j = 0; j < size; j++)
        {
            printf("#%d\t%lf\n", j + 1, time[i][j]);
            if (i == 0) sum1 += time[i][j];
            else sum2 += time[i][j];
        }
    }
	avg1 = (double) (sum1 / size);
	avg2 = (double) (sum2 / size);
	printf("\nAvg for 1st part:\t%f\nAvg for 2nd part:\t%f\n", avg1, avg2);
	printf("\nAvg of entire file:\t%f\n", avg1 + avg2);
}

int main()
{
    int listeningSocket;
	char ack[] = AUTHENTICATION;

    signal(SIGPIPE, SIG_IGN); // on linux to prevent crash on closing socket

    // Open the listening (server) socket
    if ((listeningSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("Could not create listening socket");
        return -1;
	}

    // Reuse the address if the server socket on was closed
	// and remains for 45 seconds in TIME-WAIT state till the final removal.
	int enableReuse = 1;
	if (setsockopt(listeningSocket, SOL_SOCKET, SO_REUSEADDR, & enableReuse, sizeof(int)) < 0)
	{
		perror("setsockopt() failed");
        return -1;
	}

    // "sockaddr_in" is the "derived" from sockaddr structure
	// used for IPv4 communication. For IPv6, use sockaddr_in6
	
	struct sockaddr_in serverAddress;
	memset(&serverAddress, 0, sizeof(serverAddress));

	serverAddress.sin_family = AF_INET;
	serverAddress.sin_addr.s_addr = INADDR_ANY;
	serverAddress.sin_port = htons(SERVER_PORT);  //network order

	// Bind the socket to the port with any IP at this port
	if (bind(listeningSocket, (struct sockaddr*) & serverAddress, sizeof(serverAddress)) == -1)
	{
		perror("Bind failed");
		close(listeningSocket);
		return -1;
	}

	printf("Bind() success\n");

    // Make the socket listening; actually mother of all client sockets.
	if (listen(listeningSocket, 1) == -1)  // 1 is a Maximum size of queue connection requests
	{
		perror("listen() failed");
		close(listeningSocket);
		return -1;
	}

    // Accept and wait for incoming connection
	printf("Waiting for incoming TCP-connections...\n");

	struct sockaddr_in clientAddress;  //
	socklen_t clientAddressLen = sizeof(clientAddress);
	memset(&clientAddress, 0, sizeof(clientAddress));
	clientAddressLen = sizeof(clientAddress);
	int clientSocket = accept(listeningSocket, (struct sockaddr*) & clientAddress, &clientAddressLen);
	if (clientSocket == -1)
	{
		perror("Listen failed");
		close(clientSocket);
		close(listeningSocket);
		return -1;
	}

    printf("A new client connection accepted\n");

    // For saving the results
    double time[2][SIZE];
	uint counter = 0;
	int run = 1;
    while (1)
    { 
		int readSize;
		char recvMsg[1024+1];
		struct timeval start, stop;
		double start_ms, stop_ms;
		memset(&recvMsg[0], 0, sizeof(recvMsg));
		readSize = recv(clientSocket, recvMsg, 1024, 0);

		if (counter >= SIZE)
		{
			int x = SIZE;
			printf("Reached max measures (%d)\n", x);
			printBeforeExit(time, counter);
			printf("Exiting...");
			break;
		}

		// Receive 1st part
		if (strstr(recvMsg, "EXIT"))
		{
			printf("\nSender reqouested EXIT\n");
			printBeforeExit(time, counter);
			break;
		}
		else
		{
			char start1[] = "START";
			send(clientSocket, start1, sizeof(start) - 1, 0);
		}
		
        /*while (!(strstr(recvMsg, "START")))
        {
            readSize = recv(clientSocket, recvMsg, 1024, 0);
            recvMsg[readSize] = '\0';
            if ((strstr(recvMsg, "EXIT")))
            {
				printf("Sender reqouested EXIT\n");
				printBeforeExit(time, counter);
				run = 0;
				break;
            }
        }*/

		//Exit the loop
		//if (!run) break;

        printf("START receiving data1\n");
        // Get the START time
        gettimeofday(&start, NULL);
		start_ms = (start.tv_sec * 1000) + (start.tv_usec / 1000);
		while (1)
		{
			memset(&recvMsg[0], 0, sizeof(recvMsg));
			readSize = recv(clientSocket, recvMsg, 1024, 0);
			if (strstr(recvMsg, "**END**") != NULL) break;
		}

        /*while (strstr(recvMsg, "**END**") != NULL)
		{
			printf("Received: %s\n", recvMsg);
			readSize = recv(clientSocket, recvMsg, 1024, 0);
            recvMsg[readSize] = '\0';
		}*/

		// Get the END time
		gettimeofday(&stop, NULL);
		stop_ms = (stop.tv_sec * 1000) + (stop.tv_usec / 1000);
		//printf("stop_ms %f\n", stop_ms);
		time[0][counter] = stop_ms - start_ms;

		printf("1st part received\n");

		// Send back the authentication
		if (send(clientSocket, ack, sizeof(ack), 0) == -1)
		{
            perror("Error: faild to send authentication msg");
			break;
		}

		printf("Authentication sent, waiting for 2nd part...\n");

		// Receive 2nd part
		//readSize = recv(clientSocket, recvMsg, 1024, 0);
		//recvMsg[readSize] = '\0';
		/*while (strstr(recvMsg, "START") == NULL)
        {
            readSize = recv(clientSocket, recvMsg, 1024, 0);
            recvMsg[readSize] = '\0';
        }*/
        char buf1[1025];
		memset(&buf1[0], 0, sizeof(buf1));
		recv(clientSocket, buf1, 1024, 0);
		char start1[] = "START";
		send(clientSocket, start1, sizeof(start) - 1, 0);

		printf("START receiving data\n");

        // Get the START time
        gettimeofday(&start, NULL);
		start_ms = (start.tv_sec * 1000) + (start.tv_usec / 1000);
		while (1)
		{

			memset(&buf1[0], 0, sizeof(buf1));
			readSize = recv(clientSocket, buf1, 1024, 0);
			char* strpt;
			strpt = strstr(buf1, "**END**");
			if (strpt != 0) break;
		}

        /*while (strstr(buf1, "**END**") != NULL)
		{
			char buf[1025];
			recv(clientSocket, buf, 1024, 0);
			if (strstr(buf, "**END**") != NULL) break;
		}*/

		gettimeofday(&stop, NULL);
		stop_ms = (stop.tv_sec * 1000) + (stop.tv_usec / 1000);
		//printf("stop_ms %f\n", stop_ms);
		time[1][counter] = stop_ms - start_ms;
		printf("2nd part received\n");
		counter++;
    }
	printf("\nExiting program...\n");

	// Close sockets
	close(clientSocket);
	close(listeningSocket);

	return 0;
}