#include "packet.h"

int sockfd;
struct sockaddr_in send;
socklen_t send_len = sizeof(send);
float drop;

void send_packet(Packet *packet) {
    char buffer[sizeof(Packet)];
    serialize_packet(packet, buffer);
    sendto(sockfd, buffer, sizeof(Packet), 0, (struct sockaddr *)&send, send_len);
    log_event("패킷 전송함.", packet);
}

void receive_file() {
    char buffer[sizeof(Packet)];
    Packet packet;
    int expected_seq_num = 0;

    while (1) {
        if (recvfrom(sockfd, buffer, sizeof(Packet), 0, (struct sockaddr *)&sender_addr, &sender_addr_len) > 0) {
            deserialize_packet(buffer, &packet);
            if (((float)rand()/RAND_MAX) < drop) {
                log_event("Dropped DATA", &packet);
                continue;
            }
            log_event("데이터 받음.", &packet);
            if (packet.type == EOT) {
                log_event("EOT 받음.", &packet);
                break;
            }
            if (packet.seq_num == expected_seq_num) {
                expected_seq_num++;
            }
     
            Packet ack_packet = {ACK, NONE, 0, packet.seq_num, 0, ""};
            serialize_packet(&ack_packet, buffer);
            sendto(sockfd, buffer, sizeof(Packet), 0, (struct sockaddr *)&sender_addr, sender_addr_len);
            log_event("ACK 보냄.", &ack_packet);
        }
    }
}
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <receiver_port> <data_drop_prob>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int receive_port = atoi(argv[1]);
    data_drop_prob = atof(argv[2]);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("소켓 만들기 실패.");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in new_receive;
    memset(&new_receive, 0, sizeof(new_receive));
    new_receive.sin_family = AF_INET;
    new_receive.sin_port = htons(receive_port);
    new_receive.sin_addr.s_addr = INADDR_ANY;


    if (bind(sockfd, (const struct sockaddr *)&new_receive, sizeof(new_receive)) < 0) {
        perror("합치기 실패.");
        exit(EXIT_FAILURE);
    }
    receive_file();
    close(sockfd);
    return 0;
}
