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

#define STRING_SIZE 1024
#define BUFFER_SIZE 80
#define SERVER_PORT 11235

struct udpPacket {
  unsigned int length;
  unsigned int sequenceNumber;
  char data[80];
};

//generate random number to simulate packet loss
int SimulateLoss(float packetLoss){
  double randnum = (double)rand()/(double)RAND_MAX;
  if(randnum < packetLoss){
    return 1;
  }
  return 0;
}

int main(int argc, char *argv[]) {
  int userTimeout;
  float userPacketLoss;  
  int generatedPacketCount = 0;
  int generatedBytesCount = 0;
  int transmittedPacketCount = 0;
  int droppedPacketCount = 0;
  int successPacketCount = 0;
  int ACKReceiveCount = 0;
  int timeoutCount = 0;
  
  if( argc < 3 ){ //print help text
    printf("Try: ./udpserver [timeout] [packet loss ratio]\n");
    exit(1);
  }
  if(sscanf (argv[1], "%i", &userTimeout) != 1) { 
    fprintf(stderr, "Failed to read in timeout value");
    exit(1);
  }
  if(sscanf (argv[2], "%f", &userPacketLoss)!=1) { 
    fprintf(stderr, "Failed to read in packet loss value");
    exit(1);
  }  

  printf("Starting server with timeout: %d, and packet loss ratio: %0.1f\n\n", userTimeout, userPacketLoss);

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
  server_port = SERVER_PORT; /* Server will listen on this port */
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
  
  struct udpPacket receivedPacket;
  bytes_recd = recvfrom(sock_server, &receivedPacket, sizeof(receivedPacket), 0, (struct sockaddr *) &client_addr, &client_addr_len);
  //make sure receive was good
  if(bytes_recd < 0){
    perror("Filename data receive error");
    exit(1);
  }

  //copy filename string out of received packet
  unsigned int nameLength = ntohs(receivedPacket.length);
  char fileName[nameLength];
  strncpy(fileName, receivedPacket.data, nameLength); 
  fileName[nameLength] = '\0';

  //setup socket timeout
  double micros = pow(10, userTimeout);
  int millis = micros/1000;
  struct timeval toConvert;
  toConvert.tv_sec = millis/1000;
  toConvert.tv_usec = millis%1000;
  setsockopt(sock_server, SOL_SOCKET, SO_RCVTIMEO, (const char*)&toConvert, sizeof toConvert);

  //connection variables
  unsigned short sequenceCount = 0;
  FILE *fp = NULL;
  fp = fopen(filename, "r");

  if(fp){
    char *buffer = calloc(1,BUFFER_SIZE);

    while(fgets(buffer,BUFFER_SIZE,fp) != NULL){
      /* prepare the message to send */
      int bufferLength = strlen(buffer);

      struct udpPacket newPacket;
      newPacket.length = htons(bufferLength);
      newPacket.sequenceNumber = htons(sequenceCount);
      strncpy(newPacket.data, buffer, bufferLength);

      printf("Packet %d generated for transmission with %d data bytes\n", sequenceCount, bufferLength);
      generatedPacketCount++;
      generatedBytesCount = generatedBytesCount + bufferLength;
      
      int ACKReceived = 0;
      while(!ACKReceived){
        transmittedPacketCount++;
        if(!SimulateLoss(userPacketLoss)){
          //send data packet
          bytes_sent = sendto(sock_server, &newPacket, bufferLength+4, 0, (struct sockaddr*) &client_addr, client_addr_len);
          if(bytes_sent < 0){
            perror("Data Packet sending error");
            exit(1);
          }

          //print stats
          printf("Packet %d successfully transmitted with %d data bytes\n", sequenceCount, bytes_sent-4);
          successPacketCount++;
        }
        else{
          //print stats
          printf("Packet %d lost\n", sequenceCount);
          droppedPacketCount++;
        }

        //clear buffer
        memset(buffer, 0, BUFFER_SIZE);

        //wait for ACK packet
        unsigned int recv_ack;
        bytes_recd = recvfrom(sock_server, &recv_ack, sizeof(recv_ack), 0, (struct sockaddr *) 0, (int *) 0);
        if(bytes_recd < 0){
          if( errno == EAGAIN || errno == EWOULDBLOCK ){
            //print stats
            printf("Timeout expired for packet numbered %d\n", sequenceCount);
            printf("Packet %d generated for re-transmission with %d data bytes\n", sequenceCount, bufferLength);
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
          if( recv_ack == sequenceCount ){ //verify sequence number
            printf("ACK %d received\n", recv_ack);
            ACKReceiveCount++;
            sequenceCount = 1 - sequenceCount;
            ACKReceived = 1;
          }  
        } 
      }
    }
    //free memory to cleanup
    free(buffer);
    fclose(fp);
  }
  else{
    //Print reason why file couldn't be opened
    perror("Error opening file");

    char notFoundMsg[16] = "FILE NOT FOUND\n";
    int bufferLength = 16;
    
    struct udpPacket newPacket;
    newPacket.length = htons(bufferLength);
    newPacket.sequenceNumber = htons(sequenceCount);
    strncpy(newPacket.data, notFoundMsg, bufferLength);

    bytes_sent = sendto(sock_server, &newPacket, bufferLength+4, 0, (struct sockaddr*) &client_addr, client_addr_len);
    if(bytes_sent < 0){
      perror("Not Found Packet sending error");
      exit(1);
    }

    printf("Packet %d transmitted with %d data bytes\n", sequenceCount, bytes_sent-4);
    sequenceCount = 1 - sequenceCount;
  }

  struct udpPacket endTransmission;
  endTransmission.length = htons(0);
  endTransmission.sequenceNumber = htons(sequenceCount);

  bytes_sent = sendto(sock_server, &endTransmission, 4, 0, (struct sockaddr*) &client_addr, client_addr_len);
  if(bytes_sent < 0){
    perror("EOT Packet sending error");
    exit(1);
  }

  printf("\n");
  printf("End of transmission packet with sequence number %d transmitted\n", sequenceCount);
  
  //Print final results
  printf("Number of data packets generated for transmission: %d\n", generatedPacketCount);
  printf("Total number of data bytes generated for transmission, initial transmission only: %d\n", generatedBytesCount);
  printf("Total number of data packets generated for retransmission: %d\n", transmittedPacketCount);
  printf("Number of data packets dropped due to loss: %d\n", droppedPacketCount);
  printf("Number of data packets transmitted successfully: %d\n", successPacketCount);
  printf("Number of ACKs received: %d\n", ACKReceiveCount);
  printf("Number of timeouts expired: %d\n", timeoutCount);
  
  /* close the socket */
  close(sock_server);
  exit(0);
}
