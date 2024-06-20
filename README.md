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
#include "packet.h"

int udp_socket;
struct sockaddr_in receive_add;
socklen_t receive_add_length = sizeof(receive_add);
int retransmission;
float drop_ack;

void retransmission(int sig) {
      if (sig == SIGALRM) {
        ssthresh = cwnd / 2 > 1 ? cwnd / 2 : 1;
        cwnd = INITIAL_CWND;
        duplicate_ack_count = 0;
        next_seq_num = base_seq_num;
        log_cwnd("Timeout occurred", cwnd);
    }
    
}

void send_file_with_congestion_control(const char *file_path) {
    FILE *file = fopen(file_path, "rb");
    if (!file) {
        perror("파일 열기 실패");
        exit(EXIT_FAILURE);
    }

    int current_seq_num = 0;
    char buffer[sizeof(Packet)];
    Packet data_packet;
    size_t read_bytes;

    signal(SIGALRM, retransmission);

    int cwnd = 1;
    int ssthresh = 16;
    int duplicate_ack_count = 0;
    int next_seq_num = 0;
    int ack_num = 0;
    int base = 0;

    while ((read_bytes = fread(data_packet.payload, 1, MAX_PAYLOAD_SIZE, file)) > 0) {
        data_packet.type = DATA;
        data_packet.flag = NONE;
        data_packet.seq_num = current_seq_num;
        data_packet.ack_num = 0;
        data_packet.length = read_bytes;

        serialize_packet(&data_packet, buffer);
        sendto(udp_socket, buffer, sizeof(Packet), 0, (struct sockaddr *)&receive_add, receive_add_length);
        log_event("DATA 패킷 전송", &data_packet);
        log_cwnd("CWND 업데이트", cwnd);

        alarm(retransmission);

        Packet ack_packet;
        while (1) {
            if (recvfrom(udp_socket, buffer, sizeof(Packet), 0, (struct sockaddr *)&receive_add, &receive_add_length) < 0) {
                if (errno == EINTR) {
                    
                    sendto(udp_socket, buffer, sizeof(Packet), 0, (struct sockaddr *)&receive_add, receive_add_length);
                    log_event("데이터 패킷 재전송", &data_packet);
                    alarm(retransmission);
                }

            } else {
                deserialize_packet(buffer, &ack_packet);
                if (ack_packet.type == ACK && ack_packet.ack_num == current_seq_num) {
                    log_event("ACK 패킷 수신", &ack_packet);
                    break;
                }
            }
        }

        alarm(0);
        current_seq_num++;
        next_seq_num = current_seq_num;

        // Congestion control
        if (cwnd < ssthresh) {
            // Slow start
            cwnd++;

        } else {
            // Congestion avoidance
            cwnd += 1 / cwnd;
        }
    }
    // FIN 패킷 전송하기.
    data_packet.type = FIN;
    data_packet.flag = FIN;
    data_packet.seq_num = current_seq_num;
    data_packet.ack_num = 0;
    data_packet.length = 0;

    serialize_packet(&data_packet, buffer);
    sendto(udp_socket, buffer, sizeof(Packet), 0, (struct sockaddr *)&receive_add, receive_add_length);
    log_event("FIN 패킷 전송", &data_packet);

    fclose(file);
}

int main(int argc, char *argv[]) {
    if (argc != 7) {
        fprintf(stderr, "Usage: %s <sender_port> <receiver_ip> <receiver_port> <timeout_interval> <filename> <ack_drop_prob>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int sender_port = atoi(argv[1]);
    char *receive_ip = argv[2];
    int receive_port = atoi(argv[3]);
    retransmission = atoi(argv[4]);
    char *file_path = argv[5];
    drop_ack = atof(argv[6]);

    udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket < 0) {
        perror("소켓 생성 실패");
        exit(EXIT_FAILURE);
    }

    memset(&receive_add, 0, sizeof(receive_add));
    receive_add.sin_family = AF_INET;
    receive_add.sin_port = htons(receive_port);
    inet_pton(AF_INET, receive_ip, &receive_add.sin_addr);

    send_file_with_congestion_control(file_path);
    close(udp_socket);
    return 0;
}
//sender.c에서 SYN-ACK 대기,ACK 패킷 전송,FIN 패킷 전송 구현
