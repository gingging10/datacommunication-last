# datacommunication-last

//우선, receiver.c 파일에서 첫 번째 줄인 #include "packet.h"를 제대로 불러오지 않으면 오류가 발생하는데 다음과 같은 선언을 넣어주어야 정상 실행 된다.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#define DATA 0
#define ACK 1
#define EOT 2
#define MAX_PAYLOAD_SIZE 1000

typedef struct {
    int type;
    int seq_num;
    int ack_num;
    int length;
    char payload[MAX_PAYLOAD_SIZE];
} Packet;

void serialize_packet(Packet *pkt, char *buffer) {
    memcpy(buffer, pkt, sizeof(Packet));
}

void deserialize_packet(char *buffer, Packet *pkt) {
    memcpy(pkt, buffer, sizeof(Packet));
}

void log_event(const char *event, Packet *pkt) {
    time_t now;
    time(&now);
    char *time_str = ctime(&now);
    time_str[strlen(time_str) - 1] = '\0'; // remove newline
    printf("[%s] %s: Type=%d, SeqNum=%d, AckNum=%d, Length=%d\n", time_str, event, pkt->type, pkt->seq_num, pkt->ack_num, pkt->length);
}
//
