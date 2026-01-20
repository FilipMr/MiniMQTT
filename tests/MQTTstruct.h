#ifndef MQTTSTRUCT_H
#define MQTTSTRUCT_H

#define MAXCLIENTS 100
#define MAX_TOPIC_SIZE 200
#define MAX_PAYLOAD_SIZE 1024
#include    <stdio.h>

typedef enum 
{
    STATUS_PACKET = 0,
    INFO_PACKET,
    DATA_PACKET,
    CONTROL_PACKET
} packetType;

typedef struct
{
    char client_id[MAXCLIENTS];
    packetType type;
    char payload[MAX_PAYLOAD_SIZE];
} MQTTpacket;

typedef struct
{
    char client_id[MAXCLIENTS];
    packetType type;
    char answer[10];
    char topic[MAX_TOPIC_SIZE];
    char payload[MAX_PAYLOAD_SIZE];
} cliAnswer;

void packPacket(MQTTpacket *packet, char* buff, int* size)
{
    *size = sizeof(MQTTpacket);
    memcpy(buff, packet, *size);
}

void publishPacket(int fd, const char* topic, MQTTpacket* packet)
{
    char buff[sizeof(MQTTpacket)];
    int len = 0;

    packPacket(packet, buff, &len);

    if(write(fd, buff, len) < 0)
    {
        fprintf(stderr, "write() MQTTpacket error: %s", strerror(errno));
    }

}

#endif