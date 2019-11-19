/* udp_client.c */
/*By Michael Guerrero & Stephen Eaton*/

#include <stdio.h>          /* for standard I/O functions */
#include <stdlib.h>         /* for exit */
#include <string.h>         /* for memset, memcpy, and strlen */
#include <netdb.h>          /* for struct hostent and gethostbyname */
#include <sys/socket.h>     /* for socket, sendto, and recvfrom */
#include <netinet/in.h>     /* for sockaddr_in */
#include <unistd.h>         /* for close */
#include <errno.h>          // for timeout
#include <time.h>           // for time seed for rand()

#define STRING_SIZE 1024

#define SERVER_HOSTNAME localhost
#define SERVER_PORT 11235

struct udp_pkt {
  unsigned int length;
  unsigned int sequenceNum;
  char data[80];
};

int simulateACKLoss(float ackLoss);

int main(int argc, char *argv[]) {

  // Seed random number
  srand(time(0));

  //Initialize Variables
  int sock_client;  /* Socket used by client */
  struct sockaddr_in client_addr;  /* Internet address structure that
                                        stores client address */
  unsigned short client_port;  /* Port number used by client (local port) */

  struct sockaddr_in server_addr;  /* Internet address structure that
                                        stores server address */
  struct hostent * server_hp;      /* Structure to store server's IP
                                        address */
  int bytes_sent, bytes_recd; /* number of bytes sent or received */
  int successfulPackets;
  int dupPackets;
  int nonDupSucc;
  int numBytesRecieved;
  int acksSent;
  int acksLost;
  char fileName[STRING_SIZE];
  float input_ackLoss;
  int eot;
  FILE *fp = fopen("out.txt", "wb");

  //prints if user incorrectly tries to run the client.

  if( argc < 3 ){
    printf("Try ./udpclient [file name] [ack loss ratio]\n");
    exit(1);
  }

  //takes in file name

  if(strlen(argv[1]) >= STRING_SIZE) {
    fprintf(stderr, "File name is too long");
    exit(1);
  }
  strcpy(fileName, argv[1]);

  //takes in ALR

  if(sscanf (argv[2], "%f", &input_ackLoss)!=1) {
    fprintf(stderr, "Error reading ACK loss value");
    exit(1);
  }

  printf("Connecting to server to retrieve %s, with an ALR of %0.1f\n\n", fileName, input_ackLoss);

  /* open a socket */

  if ((sock_client = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
    perror("Client: can't open datagram socket\n");
    exit(1);
  }

  /* initialize client address information */

  memset(&client_addr, 0, sizeof(client_addr));
  client_addr.sin_family = AF_INET;
  client_addr.sin_addr.s_addr = htonl(INADDR_ANY); /* This allows choice of any host interface, if more than one are present */
  client_addr.sin_port = htons(client_port);

  /* bind the socket to the local client port */

  if (bind(sock_client, (struct sockaddr *) &client_addr, sizeof (client_addr)) < 0) {
    perror("Client: can't bind to local address\n");
    close(sock_client);
    exit(1);
  }

  /* end of local address initialization and binding */

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

  /* send fileName */
  unsigned int sequenceNum = 0;
  unsigned int buffLen = strlen(fileName);

  struct udp_pkt newpkt;
  newpkt.length = htons(buffLen);
  newpkt.sequenceNum = htons(sequenceNum);
  strncpy(newpkt.data, fileName, buffLen);

  bytes_sent = sendto(sock_client, &newpkt, buffLen+4, 0, (struct sockaddr *) &server_addr, sizeof (server_addr));
  if(bytes_sent < 0){
    perror("fileName Packet sending error");
    exit(1);
  }

  //Run till eot packet is recieved
  while(!eot){
    /* get response from server */

    struct udp_pkt receivePkt;
    bytes_recd = recvfrom(sock_client, &receivePkt, sizeof(receivePkt), 0, (struct sockaddr *) 0, (int *) 0);
    if(bytes_recd < 0){
      perror("Data Packet received with error");
      exit(1);
    }
    unsigned int length = ntohs(receivePkt.length);
    unsigned int sequenceNum = ntohs(receivePkt.sequenceNum);

    //check if packet received is the eot packet
    if(length == 0){
      printf("\nEnd of Transmission Packet received with sequence number %d \n", sequenceNum);
      eot = 0;
    }
    else {
      successfulPackets++;
      if(sequenceNum == sequenceNum){
        printf("Packet %d received with %d data bytes\n", sequenceNum, bytes_recd-4);
        nonDupSucc++;
        numBytesRecieved += length;
        if(fp){
          fwrite(receivePkt.data,1,length, fp);
          printf("Packet %d delivered to user\n", sequenceNum);
        }
        else{
          perror("Error writing to file");
        }
        sequenceNum = 1 - sequenceNum;
      }
      else{
        printf("Duplicate packet %d received with %d data bytes\n", sequenceNum, bytes_recd-4);
        dupPackets++;
      }

      //send ACK
      printf("ACK generated for transmission with sequence number %d\n", sequenceNum);
      acksSent++;
      if(!simulateACKLoss(input_ackLoss)){
        uint16_t ack_seq = htons(sequenceNum);
        bytes_sent = sendto(sock_client, &ack_seq, sizeof(ack_seq), 0, (struct sockaddr *) &server_addr, sizeof (server_addr));
        if(bytes_sent < 0){
          perror("ACK Packet sending error");
          exit(1);
        }
        printf("ACK %d successfully transmitted\n", sequenceNum);
        acksSent++;
      }
      else{
        printf("ACK %d lost\n", sequenceNum);
        acksLost++;
      }

    }
  }
  if(fp){
    fclose(fp);
  }

  //print stats
  printf("Total number of data packets received successfully %d\n", successfulPackets);
  printf("Number of duplicate data packets received: %d\n", dupPackets);
  printf("Number of data packets received successfully, not including duplicates: %d\n", nonDupSucc);
  printf("Total number of data bytes received which are delivered to user: %d\n", numBytesRecieved);
  printf("Number of ACKs transmitted without loss: %d\n", acksSent);
  printf("Number of ACKs generated, but dropped due to loss: %d\n", acksLost);
  printf("Total number of ACKs generated: %d\n", acksSent);

  /* close the socket */
  close(sock_client);
}

//function that simulates ack loss compared to a random number
int simulateACKLoss(float ackLoss){
  double randnum = (double)rand() / (double)RAND_MAX;
  if(randnum < ackLoss){
    return 1;
  }
  return 0;
}
