# datacommunication-last

//우선, receiver.c 파일에서 첫 번째 줄인 #include "packet.h"를 제대로 불러오지 않으면 오류가 발생하는데 다음과 같은 선언을 넣어주어야 정상 실행 된다.
//receiver.c 파일을 작성하여 TCP 3-way 핸드셰이킹, 연결 종료, 그리고 수신 기능을 구현하였다.
 argc != 2로 명령줄 인자가 두 개인 경우를 처리
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
//sender.c에서는 TCP Reno 혼잡 제어 알고리즘을 기반으로 cwnd를 조정, 파일 전송 중에 패킷 손실과 같은 이벤트를 처리함.
//TCP Reno의 혼잡 제어 메커니즘을 구현을 하고자 혼잡 윈도우(cwnd)는 초기에 1로 시작하고, 매번 성공적으로 ACK를 받을 때마다 1씩 증가,혼잡 윈도우(cwnd)가 혼잡 회피 임계값(ssthresh)에 도달하면, 혼잡 윈도우(cwnd)는 매 RTT마다 1씩 증가와 같은 방식을 이용함.
