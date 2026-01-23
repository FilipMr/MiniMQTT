#ifndef MQTTSTRUCT_H
#define MQTTSTRUCT_H

#define MAXCLIENTS 100
#define MAX_TOPIC_SIZE 200
#define MAX_PAYLOAD_SIZE 1024
#include    <stdio.h>

/// @brief enum to determine what type of packet is send
typedef enum 
{
    STATUS_PACKET = 0,
    INFO_PACKET,
    DATA_PACKET,
    CONTROL_PACKET
} packetType;

/// @brief struct, determine packet which is send from SERVER to CLIENT
typedef struct
{
    char client_id[MAXCLIENTS];
    packetType type;
    char topic[MAX_TOPIC_SIZE];
    
    // anything you need/want :) 
    char payload[MAX_PAYLOAD_SIZE];
} MQTTpacket;

/// @brief struct, determine packet which CLIENT send to SERVER
typedef struct
{
    char client_id[MAXCLIENTS];
    packetType type;
    char answer[10];
    // add something that helps to send back what client want 
    char topic[MAX_TOPIC_SIZE];
    char payload[MAX_PAYLOAD_SIZE];
} cliAnswer;



/// to ponizej było do wczesniejszej wersji, bez wysylania structow tylko bezposrednio ladowanie do buffora i wysylka z niego, bez pól structow  

// void packPacket(MQTTpacket *packet, char* buff, int* size)
// {
//     *size = sizeof(MQTTpacket);
//     memcpy(buff, packet, *size);
// }

// void publishPacket(int fd, const char* topic, MQTTpacket* packet)
// {
//     char buff[sizeof(MQTTpacket)];
//     int len = 0;

//     packPacket(packet, buff, &len);

//     if(write(fd, buff, len) < 0)
//     {
//         fprintf(stderr, "write() MQTTpacket error: %s", strerror(errno));
//     }

// }


#endif
