#ifndef MQTTSTRUCT_H
#define MQTTSTRUCT_H

#define MAXCLIENTS 100
#define MAX_PAYLOAD_SIZE 1024
#include    <stdio.h>

typedef enum 
{
    STATUS_PACKET = 0,
    DATA_PACKET,
    CONTROL_PACKET
} packetType;

typedef struct
{
    char client_id[MAXCLIENTS];
    packetType type;
    char payload[MAX_PAYLOAD_SIZE];
} MQTTpacket;

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