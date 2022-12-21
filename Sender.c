#include <stdio.h>
#include <string.h> 
#include <netinet/tcp.h> 
#include <stdlib.h> 
#include <errno.h> 
#include <string.h> 
#include <sys/types.h> 
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#define SERVER_PORT 5060
#define SERVER_IP_ADDRESS "127.0.0.1"
#define FILE_NAME "1mb.txt"
#define ID_1 205439649
#define ID_2 315393702

/* change the Congestion Control of the given socket
*  option = 1 for "cubic" (the default)
*  option = 2 for "reno"
*/
int changeCC(int sock, int option)
{
    int returnval;
    char buf[10];
    socklen_t len;

    // Change by the given option
    switch (option)
    {
    case (1):
        strcpy(buf, "cubic");
        len = strlen(buf);
        return setsockopt(sock, IPPROTO_TCP, TCP_CONGESTION, buf, len);
    
    case (2):
        strcpy(buf, "reno");
        len = strlen(buf);
        return setsockopt(sock, IPPROTO_TCP, 13, buf, len);
        
    default:
        return 0;
    }
}

// sends the content of *buffer* to the given *sock*. returns num of btes sent, -1 if somthing went wrong.
// *size* is the size of the buffer
int sendArray(char* buffer, int sock, int size)
{
    int sent = 0, i = 0;
    for (; i < size; i = i + sent)
    {
        sent = send(sock, &buffer[i], size, 0);
        if (sent == -1) return -1;
    }
    return i;
}

int main()
{
    FILE *fd;
    long fsize;
    //buffers to read the file into.
    //size of file should be 1mb so 1024 to each should be enough
    char *buffer1, *buffer2;
    const char eom[] = "**END**", exitmsg[] = "EXIT", startmsg[] = "START";
    uint xorID = (ID_1 % 10000) ^ (ID_2 % 10000);
    char ack[10];
    printf("xor: %d\n", xorID);
    memset(ack, 0, sizeof(ack));
    sprintf(ack, "%d", xorID);

    //open file
    fd = fopen(FILE_NAME, "rb");
    if (!fd )
    {
        perror ("Error opening file");
        return (-1);
    }

    //get the size of the file
    fseek(fd, 0, SEEK_END);
    fsize = ftell(fd);
    rewind(fd);

    int buf_size = (fsize/2);

    // allocate memory for the 1st part of the file
    buffer1 = (char*) malloc(buf_size +1);
    if (!buffer1)
    {
        fclose(fd);
        perror("Memory alloc fails");
        return (-1);
    }

    // allocate memory for the 2nd part of the file
    buffer2 = (char*) malloc(buf_size +1 );
    if (!buffer2)
    {
        fclose(fd);
        perror("Memory alloc fails");
        return (-1);
    }

    memset(buffer1, 0, buf_size+1);
    //read 1st part
    if (fread(buffer1, buf_size, 1, fd) != 1)
    {
        fclose(fd);
        free(buffer1);
        free(buffer2);
        perror("Read 1st part failed");
        return (-1);
    }

    //read 2nd part
    memset(buffer2, 0, buf_size+1);
    if (fread(buffer2, buf_size, 1, fd) != 1)
    {
        fclose(fd);
        free(buffer1);
        free(buffer2);
        perror("Read 2nd part failed");
        return (-1);
    }

    //open socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);

	if (sock == -1)
	{
        fclose(fd);
        free(buffer1);
        free(buffer2);
		close(sock);
		perror("Could not create socket");
		return -1;
	}
	else printf("Socket %d created\n", sock);
	
	struct sockaddr_in serverAddress;
	memset(&serverAddress, 0, sizeof(serverAddress));
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(SERVER_PORT);
	int rval = inet_pton(AF_INET, (const char*)SERVER_IP_ADDRESS, &serverAddress.sin_addr);
	if (rval <= 0)
	{
        fclose(fd);
        free(buffer1);
        free(buffer2);
		close(sock);
		perror("inet_pton() failed");
		close(sock);
		return -1;
	}

	// Make a connection to the server with socket SendingSocket.
	printf("Trying to connect...\n");
	if (connect(sock, (struct sockaddr*) & serverAddress, sizeof(serverAddress)) == -1)
	{
        fclose(fd);
        free(buffer1);
        free(buffer2);
		close(sock);
		perror("connect() failed");
		return -1;
	}

	printf("Connected to server\n");
    int returnval = 1;
    char error[1024] = "";
    while (1)
    {
        char recvbuff[1025];
        
        //Tell the Receiver we are sending the msg
        if (send(sock, startmsg, sizeof(startmsg) - 1, 0) == -1)
        {
            strcpy(error, "Error: faild to send START msg");
            returnval = -1;
            break;
        }
        // To know the receiver is ready
        memset(recvbuff, 0, sizeof(recvbuff));
        recv(sock, recvbuff, 1024, 0);

        //send 1st prt
        printf("Sending buffer1\n");
        if ((-1) == sendArray(buffer1, sock, buf_size))
        {
            strcpy(error, "failed to send buff1");
            returnval = -1;
            break;
        }

        //send a flag so the receiver will know its the end of msg
        if (send(sock, eom, sizeof(eom) - 1, 0) == -1)
        {
            strcpy(error, "Error: faild to send eom after buffer1 sent");
            returnval = -1;
            break;
        }
        //check for ack
        printf("buffer1 sent, waitnig for Receiver authentication\n");
        recv(sock, recvbuff, sizeof(recvbuff), 0);
        if (strcmp(recvbuff, ack) != 0)
        {
            printf("Received: %s\n", recvbuff);
            strcpy(error, "Error: Receiver didn't send authentication for buffer1");
            returnval = -1;
            break;
        }
        else printf("authentication received\nchanging cc to 'reno'\n");


        //change the cc
        if (changeCC(sock, 2) != 0)
        {
            strcpy(error, "Error at changing cc to 'reno'");
            returnval = -1;
            break;
        }

        //send 2nd prt
        /*if (send(sock, startmsg, sizeof(startmsg), 0) == -1)
        {
            strcpy(error, "Error: faild to send START msg");
            returnval = -1;
            break;
        }*/
        // Tell the Receiver we are sending the msg
        if (send(sock, startmsg, sizeof(startmsg) - 1, 0) == -1)
        {
            strcpy(error, "Error: faild to send START msg");
            returnval = -1;
            break;
        }
        memset(recvbuff, 0, sizeof(recvbuff));
        // To know the receiver is ready
        recv(sock, recvbuff, 1024, 0);

        if (sendArray(buffer2, sock, buf_size) == -1)
        {
            strcpy(error, "Error: failed to send buff2");
            returnval = -1;
            break;
        }
        //send a flag so the receiver will know its the end of msg
        if (send(sock, eom, sizeof(eom), 0) == -1)
        {
            strcpy(error, "Error: faild to send eom after buffer2 sent");
            returnval = -1;
            break;
        }
        //check for ack
        printf("buffer2 sent\n");

        /*recv(sock, recvbuff, sizeof(recvbuff), 0);
        if (recvbuff != ACK)
        {
            strcpy(error, "Error: Receiver didn't send ack for buffer2");
            returnval = -1;
            break;
        }
        else printf("ack received\n");*/
         
         //User decision:
        int choice;
        printf("\n\nSend file again? (enter '1')\nExit? (enter '0')\n");
        while (scanf("%d", &choice) == EOF)
        {
            printf("Invalid choice\n");
        }
        if (choice == 0) break;
        else
        {
            printf("changing cc back to 'cubic'\n");
            if (changeCC(sock, 1) != 0)
            {
                strcpy(error, "Error at changing cc to 'cubic'");
                returnval = -1;
                break;
            }
        }
    }
    //Send an exit message to the receiver.
    send(sock, exitmsg, sizeof(exitmsg), 0);
    printf("\n****************************************************\n");
    perror(error);
    printf("****************************************************\n");
    //close and free
    fclose(fd);
    free(buffer1);
    free(buffer2);
    close(sock);

    printf("\nExiting program...\n");

    return returnval;
}
