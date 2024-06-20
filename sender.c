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
