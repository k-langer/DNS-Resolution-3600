/*
 * CS3600, Spring 2013
 * Project 3 Starter Code
 * (c) 2013 Alan Mislove
 *
 */

#include <math.h>
#include <ctype.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "3600dns.h"

/**
 * This function will print a hex dump of the provided packet to the screen
 * to help facilitate debugging.  In your milestone and final submission, you 
 * MUST call dump_packet() with your packet right before calling sendto().  
 * You're welcome to use it at other times to help debug, but please comment those
 * out in your submissions.
 *
 * DO NOT MODIFY THIS FUNCTION
 *
 * data - The pointer to your packet buffer
 * size - The length of your packet
 */
static void dump_packet(unsigned char *data, int size) {
    unsigned char *p = data;
    unsigned char c;
    int n;
    char bytestr[4] = {0};
    char addrstr[10] = {0};
    char hexstr[ 16*3 + 5] = {0};
    char charstr[16*1 + 5] = {0};
    for(n=1;n<=size;n++) {
        if (n%16 == 1) {
            /* store address for this line */
            snprintf(addrstr, sizeof(addrstr), "%.4x",
               ((unsigned int)p-(unsigned int)data) );
        }
            
        c = *p;
        if (isprint(c) == 0) {
            c = '.';
        }

        /* store hex str (for left side) */
        snprintf(bytestr, sizeof(bytestr), "%02X ", *p);
        strncat(hexstr, bytestr, sizeof(hexstr)-strlen(hexstr)-1);

        /* store char str (for right side) */
        snprintf(bytestr, sizeof(bytestr), "%c", c);
        strncat(charstr, bytestr, sizeof(charstr)-strlen(charstr)-1);

        if(n%16 == 0) { 
            /* line completed */
            printf("[%4.4s]   %-50.50s  %s\n", addrstr, hexstr, charstr);
            hexstr[0] = 0;
            charstr[0] = 0;
        } else if(n%8 == 0) {
            /* half line: add whitespaces */
            strncat(hexstr, "  ", sizeof(hexstr)-strlen(hexstr)-1);
            strncat(charstr, " ", sizeof(charstr)-strlen(charstr)-1);
        }
        p++; /* next byte */
    }

    if (strlen(hexstr) > 0) {
        /* print rest of buffer if not empty */
        printf("[%4.4s]   %-50.50s  %s\n", addrstr, hexstr, charstr);
    }
}

int main(int argc, char *argv[]) {
  /**
   * I've included some basic code for opening a socket in C, sending
   * a UDP packet, and then receiving a response (or timeout).  You'll 
   * need to fill in many of the details, but this should be enough to
   * get you started.
   */

  // process the arguments
    if (argc != 3) {
         printf("Usage: ./3600dns @<server:port> <name>\n");
        exit(-1);
    } 
   // set the default port to 53
    int port = 53;
    char* server = argv[1] + 1;
    char* name = argv[2];
    char* offset = strchr(argv[1], ':');
    if (offset) {
        *offset = 0;
        port = atoi(offset + 1);
    } 
   
  // construct the DNS request
    unsigned char * packetDNS =  (unsigned char*)calloc(MAX_IP_PACKET_SIZE, sizeof(char));
    if (!packetDNS) {
        return -1;
    }
    headerDNS_t * header =  (headerDNS_t*)calloc(1, sizeof(headerDNS_t));
    if (!header) {
        return -1;
    }
    questionDNS_t * question =  (questionDNS_t*)calloc(1, sizeof(questionDNS_t));
    if (!question) {
        return -1;
    }
    //setup the header
    header->ID = htons(QUERY_ID);
    header->RD = ~(0);
    header->QDCOUNT = htons(0x0001);
    //setup the question, QNAME is added later
    question->QTYPE = htons(0x0001);
    question->QCLASS = htons(0x0001);

    int packetSize = 0;
    //copy header into packet
    memcpy( packetDNS, header,  sizeof(headerDNS_t) );
    packetSize += sizeof(headerDNS_t);

    //copy qname into packet
    int length = strlen(name);
    char* period = NULL;
    *( name + length ) = '.';
    *( name + length + 1 ) = 0;
    while ( (period = strchr(name, '.')) != 0 ) {
        *period = 0;
        length = strlen(name);
        memcpy( packetDNS + packetSize, &length, 1 );
        packetSize++;
        memcpy( packetDNS + packetSize, name, length);
        packetSize += length;
        name = period + 1;
    }

    //copy zero byte at end of qname
    char zeroByte = 0;
    memcpy( packetDNS + packetSize, &zeroByte, 1 );
    packetSize++;

    //copy question into packet
    memcpy( packetDNS + packetSize, question, sizeof(questionDNS_t) );
    packetSize += sizeof(questionDNS_t);


   // send the DNS request (and call dump_packet with your request)
    dump_packet( packetDNS, packetSize );

   // first, open a UDP socket  
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

   // next, construct the destination address
    struct sockaddr_in out;
    out.sin_family = AF_INET;
    out.sin_port = htons( (short) port );
    out.sin_addr.s_addr = inet_addr(server);

    if (sendto(sock, packetDNS, packetSize, 0, (struct sockaddr*)&out, sizeof(out)) < 0) {
        printf("Error occured in sento\n");
        return -1;
    }

    //Clear question buffer to use for answer buffer in the future    
    memset(packetDNS,0,MAX_IP_PACKET_SIZE);
    packetSize = 0;

    // wait for the DNS reply (timeout: 5 seconds)
    struct sockaddr_in in;
    socklen_t in_len;

    // construct the socket set
    fd_set socks;
    FD_ZERO(&socks);
    FD_SET(sock, &socks);

    // construct the timeout
    struct timeval t;
    t.tv_sec = 5;
    t.tv_usec = 0;

    // wait to receive, or for a timeout
    if (select(sock + 1, &socks, NULL, NULL, &t)) {
        if (recvfrom(sock, packetDNS/*<<input buffer>>*/, MAX_IP_PACKET_SIZE/*<<input len>>*/, 0, (struct sockaddr*) &in, &in_len) < 0) {
            printf("Error occured in recvfrom\n");
            return -1;    
        }
    } else {
        // a timeout occurred
        printf("NORESPONSE");
    }

     // print out the result
     dump_packet( packetDNS, strlen(packetDNS) );

    return 0;
}
