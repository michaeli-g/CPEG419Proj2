//CPEG419-010
//Project 2 udpclient.c
//Stephen Eaton and Michael Guerrero 

#include <stdio.h>          /* for standard I/O functions */
#include <stdlib.h>         /* for exit */
#include <string.h>         /* for memset, memcpy, and strlen */
#include <netdb.h>          /* for struct hostent and gethostbyname */
#include <sys/socket.h>     /* for socket, sendto, and recvfrom */
#include <netinet/in.h>     /* for sockaddr_in */
#include <unistd.h>         /* for close */
#include <errno.h>          // for timeout
#include <time.h>           // for random time seed

#define FILE_NAME_LENGTH 1024

#define SERVER_HOSTNAME "localhost"
#define SERVER_PORT 11235

//Define the udpPacket structure
struct udpPacket {
  uint16_t packetLength;
  uint16_t packetSequence;
  char data[80];
};

//Define the simulateACKLoss function
int simulateACKLoss(float ackLoss){
  double genRandomNum = (double)rand() / (double)RAND_MAX;
  if(genRandomNum < ackLoss){
    return 1;
  }
  return 0;
}

int main(int argc, char *argv[]) {
	
  //Initialize variables that we used in this assignment	
  char fileName[FILE_NAME_LENGTH];
  float userACKLoss;
  int packetReceivedCount = 0;
  int packetDuplicateCount = 0;
  int notDuplicateCount = 0;
  int byteCount = 0;
  int ACKTransmitCount = 0;
  int ACKDropCount = 0;
  int ACKGeneratedCount = 0;
  int endCheck = 1;
  FILE *fp = fopen("out.txt", "w");
  srand(time(0)); 
  
  //Check for 3 arguments from the user when they start the program.
  //if there are not enough inputs,it tells the user the format to enter the inputs
  if( argc < 3 ){ 
    printf("Format: ./udpclient [file name] [ack loss ratio]\n");
    exit(1);
  }
  strcpy(fileName, argv[1]);

  if(sscanf (argv[2], "%f", &userACKLoss)!=1) { 
    fprintf(stderr, "Invalid ACK loss value");
    exit(1);
  }

  int sock_client;  /* Socket used by client */ 

  struct sockaddr_in client_addr;  /* Internet address structure that
                                        stores client address */
  unsigned short client_port;  /* Port number used by client (local port) */

  struct sockaddr_in server_addr;  /* Internet address structure that
                                        stores server address */
  struct hostent * server_hp;      /* Structure to store server's IP
                                        address */

  int bytes_sent, bytes_recd; /* number of bytes sent or received */

  /* open a socket */

  if ((sock_client = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
    perror("Client: can't open datagram socket\n");
    exit(1);
  }

  /* clear client address structure and initialize with client address */
  memset(&client_addr, 0, sizeof(client_addr));
  client_addr.sin_family = AF_INET;
  client_addr.sin_addr.s_addr = htonl(INADDR_ANY); /* This allows choice of
                                        any host interface, if more than one 
                                        are present */
  client_addr.sin_port = htons(client_port);

  /* bind the socket to the local client port */

  if (bind(sock_client, (struct sockaddr *) &client_addr,
           sizeof (client_addr)) < 0) {
    perror("Client: can't bind to local address\n");
    close(sock_client);
    exit(1);
  }
  
  /* initialize server address information */
  if ((server_hp = gethostbyname(SERVER_HOSTNAME)) == NULL) {
    perror("Client: invalid server hostname\n");
    close(sock_client);
    exit(1);
  }

  /* Clear server address structure and initialize with server address */
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  memcpy((char *)&server_addr.sin_addr, server_hp->h_addr, server_hp->h_length);
  unsigned short server_port = SERVER_PORT;
  server_addr.sin_port = htons(server_port);

  //Define new variables
  unsigned int sequenceNumber = 0;
  unsigned int buffLen = strlen(fileName);

  struct udpPacket newPacket;
  newPacket.packetLength = htons(buffLen);
  newPacket.packetSequence = htons(sequenceNumber);
  strncpy(newPacket.data, fileName, buffLen);

  bytes_sent = sendto(sock_client, &newPacket, buffLen+4, 0, (struct sockaddr *) &server_addr, sizeof (server_addr));
  if(bytes_sent < 0){
    perror("Filename Packet sending error");
    exit(1);
  }
  
  //Continue to run until the end of transmission is reached
  while(endCheck){
    struct udpPacket packetReceived;
    bytes_recd = recvfrom(sock_client, &packetReceived, sizeof(packetReceived), 0, (struct sockaddr *) 0, (int *) 0);
    if(bytes_recd < 0){
      perror("Data Packet receive error");
      exit(1);
    }
	
	//Define more variables
    unsigned int packetLength = ntohs(packetReceived.packetLength);
    unsigned int packetSequence = ntohs(packetReceived.packetSequence);

    //Check for the end of transmission
    if(packetLength == 0){ 
      printf("\nEnd of Transmission Packet with sequence number %d received\n", packetSequence);
      endCheck = 0;
    }
    else {
      packetReceivedCount = packetReceivedCount + 1;
      if(sequenceNumber == packetSequence){
		  
        //print stats
        printf("Packet %d received with %d data bytes\n", packetSequence, bytes_recd-4);
        notDuplicateCount = notDuplicateCount + 1;
        byteCount += packetLength;
		
        //write received data to file
        if(fp){
          fwrite(packetReceived.data,1,packetLength, fp);
          printf("Packet %d delivered to user\n", packetSequence);
        }
        else{
          perror("Error writing to file");
        }
        sequenceNumber = 1 - sequenceNumber;
      }
      else{
        printf("Duplicate packet %d received with %d data bytes\n", packetSequence, bytes_recd-4);
        packetDuplicateCount = packetDuplicateCount + 1;
      }
      
      //send ACK
      printf("ACK %d generated for transmission\n", packetSequence);
      ACKGeneratedCount = ACKGeneratedCount + 1;
      if(!simulateACKLoss(userACKLoss)){
        uint16_t ack_seq = htons(packetSequence);        
        bytes_sent = sendto(sock_client, &ack_seq, sizeof(ack_seq), 0, (struct sockaddr *) &server_addr, sizeof (server_addr));
        if(bytes_sent < 0){
          perror("ACK Packet sending error");
          exit(1);
        }
        printf("ACK %d successfully transmitted\n", packetSequence);
        ACKTransmitCount = ACKTransmitCount + 1;
      }
      else{
        printf("ACK %d lost\n", packetSequence);
        ACKDropCount = ACKDropCount + 1;
      }
    }
  }
  if(fp){
    fclose(fp);
  }

  //Print out all the data that was collected while the program was running
  printf("Total number of data packets received successfully: %d\n", packetReceivedCount);
  printf("Number of duplicate data packets received: %d\n", packetDuplicateCount);
  printf("Number of data packets received successfully, not including duplicates: %d\n", notDuplicateCount);
  printf("Total number of data bytes delivered to user: %d\n", byteCount);
  printf("Number of ACKs transmitted without loss: %d\n", ACKTransmitCount);
  printf("Number of ACKs generated but dropped due to loss: %d\n", ACKDropCount);
  printf("Total number of ACKs generated: %d\n", ACKGeneratedCount);

  /* close the socket */
  close(sock_client);
}