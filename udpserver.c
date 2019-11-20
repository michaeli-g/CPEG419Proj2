//CPEG419-010
//Project 2 udpserver.c
//Stephen Eaton and Michael Guerrero 

#include <ctype.h>          /* for toupper */
#include <stdio.h>          /* for standard I/O functions */
#include <stdlib.h>         /* for exit */
#include <string.h>         /* for memset */
#include <sys/socket.h>     /* for socket, sendto, and recvfrom */
#include <netinet/in.h>     /* for sockaddr_in */
#include <unistd.h>         /* for close */
#include <errno.h>          // for timeout
#include <time.h>           // for random time seed
#include <math.h>           // for time calculation

#define BUFFER_SIZE 80
#define UDP_PORT 11235

struct udpPacket {
  unsigned short packetLength;
  unsigned short packetSequence;
  char data[80];
};

//generate random number to simulate packet loss
int simulateLoss(float packetLoss){
  double randnum = (double)rand() / (double)RAND_MAX;
  if(randnum < packetLoss){
    return 1;
  }
  return 0;
}

int main(int argc, char *argv[]) {

  //Define all variables that we used throughout the project
  int userTimeout;
  float userPacketLoss;
  int packetGeneratedCount = 0;
  int byteCount = 0;
  int packetTransmitCount = 0;
  int packetDropCount = 0;
  int packetSuccessCount = 0;
  int numAcksReceived = 0;
  int timeoutCount = 0;
  unsigned short sequenceNumber = 0;
  FILE *fp;
  
 
  //Print message to the user showing the valid layout
  if( argc < 3 ){
    printf("Format: ./udpserver [timeout] [packet loss ratio]\n");
    exit(1);
  }
  
  //read in the timeout value, and let the user know if it's an invalid time
  if(sscanf (argv[1], "%i", &userTimeout) != 1) { 
    fprintf(stderr, "Invalid timeout value");
    exit(1);
  }
  if(sscanf (argv[2], "%f", &userPacketLoss)!=1) { //parse loss ratio input
    fprintf(stderr, "Invalid packet loss value");
    exit(1);
  }  
  
  // Use current time as seed for random generator 
  srand(time(0)); 

  int sock_server;  /* Socket on which server listens to clients */

  struct sockaddr_in server_addr;  /* Internet address structure that
                                        stores server address */
  unsigned short server_port;  /* Port number used by server (local port) */

  struct sockaddr_in client_addr;  /* Internet address structure that
                                        stores client address */
  unsigned int client_addr_len;  /* Length of client address structure */

  int bytes_sent, bytes_recd; /* number of bytes sent or received */

  /* open a socket */
  if ((sock_server = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
    perror("Server: can't open datagram socket\n");
    exit(1);
  }

  /* initialize server address information */
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl (INADDR_ANY);  /* This allows choice of
                                        any host interface, if more than one
                                        are present */
  server_port = UDP_PORT; /* Server will listen on this port */
  server_addr.sin_port = htons(server_port);

  /* bind the socket to the local server port */
  if (bind(sock_server, (struct sockaddr *) &server_addr, sizeof (server_addr)) < 0) {
    perror("Server: can't bind to local address\n");
    close(sock_server);
    exit(1);
  }

  /* wait for incoming messages in an indefinite loop */
  printf("I am here to listen ... on port %hu\n\n", server_port);
  client_addr_len = sizeof (client_addr);

  struct udpPacket packetReceived;
  bytes_recd = recvfrom(sock_server, &packetReceived, sizeof(packetReceived), 0, (struct sockaddr *) &client_addr, &client_addr_len);
  
  //make sure receive was good
  if(bytes_recd < 0){
    perror("Filename data receive error");
    exit(1);
  }

  //copy filename string out of received packet
  unsigned int fileNameLength = ntohs(packetReceived.packetLength);
  char fileName[fileNameLength];
  strncpy(fileName, packetReceived.data, fileNameLength); 
  fileName[fileNameLength] = '\0'; //add null terminator to string


  printf("Requested filename: %s\n\n", fileName);

  //setup socket timeout
  double micros = pow(10, userTimeout);
  int millis = micros/1000; 
  struct timeval tv;
  tv.tv_sec = millis/1000;
  tv.tv_usec = millis%1000;
  setsockopt(sock_server, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

 //connection variables

  fp = fopen(fileName, "r");

  if(fp){
    char *buff = calloc(1,BUFFER_SIZE);

    while(fgets(buff,BUFFER_SIZE,fp) != NULL){
      /* prepare the message to send */
      int buffLen = strlen(buff);

      struct udpPacket newPacket;
      newPacket.packetLength = htons(buffLen);
      newPacket.packetSequence = htons(sequenceNumber);
      strncpy(newPacket.data, buff, buffLen);

      printf("Packet %d generated for transmission with %d data bytes\n", sequenceNumber, buffLen);
      packetGeneratedCount = packetGeneratedCount + 1;
      byteCount = byteCount + buffLen;
      
      //loop transmit until ACK received
      int ack_recvd = 0;

      while( !ack_recvd ){
        packetTransmitCount++;
        if(!simulateLoss(userPacketLoss)){
          //send data packet
          bytes_sent = sendto(sock_server, &newPacket, buffLen+4, 0, (struct sockaddr*) &client_addr, client_addr_len);
          if(bytes_sent < 0){
            perror("Data Packet sending error");
            exit(1);
          }

          //print stats
          printf("Packet %d successfully transmitted with %d data bytes\n", sequenceNumber, bytes_sent-4);
          packetSuccessCount++;
        }
        else{
          //print stats
          printf("Packet %d lost\n", sequenceNumber);
          packetDropCount++;
        }

        //clear buffer
        memset(buff, 0, BUFFER_SIZE);

        //wait for ACK packet
        uint16_t recv_ack;
        bytes_recd = recvfrom(sock_server, &recv_ack, sizeof(recv_ack), 0, (struct sockaddr *) 0, (int *) 0);
        if(bytes_recd < 0){
          if( errno == EAGAIN || errno == EWOULDBLOCK ){
            //print stats
            printf("Timeout expired for packet numbered %d\n", sequenceNumber);
            printf("Packet %d generated for re-transmission with %d data bytes\n", sequenceNumber, buffLen);
            timeoutCount++;
          }
          else{
            perror("ACK Packet receive error");
            exit(1);
          }
        }
        else{
          //Successfully received ACK
          recv_ack = ntohs(recv_ack);
          if( recv_ack == sequenceNumber ){ 
            printf("ACK %d received\n", recv_ack);
            numAcksReceived++;
            sequenceNumber = 1 - sequenceNumber;
            ack_recvd = 1;
          }  
        } 
      }
    }
    //free memory to cleanup
    free(buff);
    fclose(fp);
  }

  //Send end of transmission packet
  struct udpPacket endingPacket;
  endingPacket.packetLength = htons(0);
  endingPacket.packetSequence = htons(sequenceNumber);

  bytes_sent = sendto(sock_server, &endingPacket, 4, 0, (struct sockaddr*) &client_addr, client_addr_len);
  if(bytes_sent < 0){
    perror("EOT Packet sending error");
    exit(1);
  }

  //print stats
  printf("\nEnd of transmission packet with sequence number %d transmitted\n", sequenceNumber);

  printf("Number of data packets generated for transmission: %d\n", packetGeneratedCount);
  printf("Total number of data bytes generated for transmission: %d\n", byteCount);
  printf("Total number of data packets generated for retransmission: %d\n", packetTransmitCount);
  printf("Number of data packets dropped due to loss: %d\n", packetDropCount);
  printf("Number of data packets transmitted successfully: %d\n", packetSuccessCount);
  printf("Number of ACKs received: %d\n", numAcksReceived);
  printf("timeouts expired count: %d\n", timeoutCount);

  /* close the socket */
  close(sock_server);
  exit(0);
}
