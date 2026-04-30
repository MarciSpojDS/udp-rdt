#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <chrono>
#include <poll.h>
#include "main.h"

#define WINDOW_SIZE 25

using namespace std;

struct __attribute__((packed)) rdt_packet {
    uint32_t seq_num;
    uint32_t ack_num;
    uint16_t data_len;
    uint16_t checksum;
    AckFlags ack_flag;
    char data[1187]; 
};

struct udp_trans {
    bool is_server = false;
    bool is_client = false;
    string port_num = ""; 
    string address = "";
    string in_file = "";
    string out_file = "";
    int timeout = 1;
    struct sockaddr_storage target_addr;
    socklen_t target_addr_len = 0;
} trans_udp;

void getHelp() {
    cout << "IPK-RDT Reliable Data Transfer" << endl;
    cout << "Usage: ./ipk-rdt [-s|-c] -p PORT [options]" << endl;
    exit(0);
}

bool send_hello(int sockfd, rdt_packet& send_pkt, AckFlags expected_flag, const udp_trans& config, struct pollfd& pfd){
    auto start_time = chrono::steady_clock::now();
    
    while (true) {
        send_pkt.checksum = 0;
        send_pkt.checksum = calc_checksum(&send_pkt, 13 + send_pkt.data_len);
        sendto(sockfd, &send_pkt, 13 + send_pkt.data_len, 0, (struct sockaddr*)&config.target_addr, config.target_addr_len);

        int poll_time = poll(&pfd, 1, 100);

        if (poll_time > 0) {
            rdt_packet recv_pkt = {};
            ssize_t bytes = recvfrom(sockfd, &recv_pkt, sizeof(recv_pkt), 0, nullptr, nullptr);
            
            if (bytes >= 13) {
                uint16_t cs = recv_pkt.checksum;
                recv_pkt.checksum = 0;
                if (calc_checksum(&recv_pkt, bytes) == cs && recv_pkt.ack_flag == expected_flag) {
                    return true;
                }
            }
        } else if (poll_time == 0) {
            if (chrono::duration_cast<chrono::seconds>(chrono::steady_clock::now() - start_time).count() >= config.timeout) {
                return false; 
            }
        } else {
            exit(ErrorCodes::NetworkError);
        }
    }
}

uint16_t calc_checksum(const void* data, size_t length) {
    const uint16_t* ptr = reinterpret_cast<const uint16_t*>(data);
    uint32_t sum = 0; 
    while (length > 1) {
        sum += *ptr++;
        length -= 2;
    }

    if (length == 1) {
        uint16_t odd_byte = 0;
        *reinterpret_cast<uint8_t*>(&odd_byte) = *reinterpret_cast<const uint8_t*>(ptr);
        sum += odd_byte;
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return static_cast<uint16_t>(~sum); 
}

void run_server(int sockfd, ostream* out_stream, const udp_trans& config) {
    struct pollfd pfd; 
    pfd.fd = sockfd;
    pfd.events = POLLIN;

    auto start_time = chrono::steady_clock::now();
    struct sockaddr_storage client_addr;
    socklen_t client_len = sizeof(client_addr);
    rdt_packet recv_packet = {};
    
    while (true)
    {   
        int poll_time = poll(&pfd, 1, 1000);

        if (poll_time > 0) {
            ssize_t bytes_received = recvfrom(sockfd, &recv_packet, sizeof(recv_packet), 0, (struct sockaddr*)&client_addr, &client_len);

            if (bytes_received > 0) {
                uint16_t received_checksum = recv_packet.checksum;
                recv_packet.checksum = 0;
                
                if (calc_checksum(&recv_packet, sizeof(recv_packet)) == received_checksum) {
                    if (recv_packet.ack_flag == AckFlags::HELLO) {
                        
                        rdt_packet ack_pkt = {};
                        ack_pkt.seq_num = 0;
                        ack_pkt.ack_num = 0;
                        ack_pkt.data_len = 0;
                        ack_pkt.ack_flag = AckFlags::HELLO_ACK;
                        
                        ack_pkt.checksum = 0;
                        ack_pkt.checksum = calc_checksum(&ack_pkt, sizeof(ack_pkt));

                        sendto(sockfd, &ack_pkt, sizeof(ack_pkt), 0,(struct sockaddr*)&client_addr, client_len);
                        
                        start_time = std::chrono::steady_clock::now();
                        break;
                    }
                }
            }
        } else if (poll_time == 0) {
            auto current_time = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count();

            if (elapsed >= config.timeout) {
                cerr << "Error Timeout" << endl;
                exit(ErrorCodes::TimeoutError); 
            }
        } else {
            cerr << "Error: Polling failed." << endl;
            exit(ErrorCodes::NetworkError);
        }
    }
    
    //DATA TRANSFER LOGIC TO BE IMPLEMENTED



}

void run_client(int sockfd, std::istream* in_stream, const udp_trans& config) {
    cout << "Starting handshake..." << endl;

    auto start_time = chrono::steady_clock::now();

    rdt_packet hello_packet = {};
    hello_packet.ack_flag = AckFlags::HELLO;
    hello_packet.seq_num = 0;
    hello_packet.ack_num = 0;
    hello_packet.data_len = 0;

    struct pollfd pfd;
    pfd.fd = sockfd;
    pfd.events = POLLIN;

    int seq_num = 1;
    int base_num = 1;

    if (!send_hello(sockfd, hello_packet, AckFlags::HELLO_ACK, config, pfd)) exit(1);
    
    bool ended = false;
    rdt_packet win_buff[WINDOW_SIZE] = {};

    while(true){
        hello_packet.checksum = 0;
        hello_packet.checksum = calc_checksum(&hello_packet, sizeof(hello_packet));
        sendto(sockfd, &hello_packet, sizeof(hello_packet), 0, (struct sockaddr*)&config.target_addr, config.target_addr_len);
        
        int poll_res = poll(&pfd, 1, 100); // 100ms retransmit timer
        
        if (poll_res > 0){
            rdt_packet recv_packet = {};
            recvfrom(sockfd, &recv_packet, sizeof(recv_packet), 0, nullptr, nullptr);

            uint16_t recv_checksum = recv_packet.checksum;
            recv_packet.checksum = 0;
 
            if (calc_checksum(&recv_packet, sizeof(recv_packet)) == recv_checksum) {
                if (recv_packet.ack_flag == AckFlags::HELLO_ACK){ break; } // Úspech!
            } else {
                cout << "Received packet with invalid checksum, ignoring." << endl;
            }
        } else if (poll_res == 0) {
            auto curr_time = chrono::steady_clock::now();
            auto elapsed_time = chrono::duration_cast<chrono::seconds>(curr_time - start_time).count();
            
            if(elapsed_time >= config.timeout){
                cerr << "Error: Timeout" << endl; 
                exit(ErrorCodes::TimeoutError);
            }
        } else {
            cerr << "Error: Polling failed." << endl;
            exit(ErrorCodes::NetworkError);
        }   
    }
    cout << "Handshake successful!" << endl;

    rdt_packet win_buff[WINDOW_SIZE] = {}; 
    bool ended = false;
    auto client_time = chrono::steady_clock::now();

    while(true) {
        while (seq_num < base_num + WINDOW_SIZE && !ended) {
            rdt_packet data_packet = {};
            in_stream->read(data_packet.data, sizeof(data_packet.data));
            streamsize bytes = in_stream->gcount();

            if (bytes > 0) {
                data_packet.seq_num = seq_num; 
                data_packet.ack_num = 0;
                data_packet.data_len = bytes;
                data_packet.ack_flag = AckFlags::DATA;

                data_packet.checksum = 0;
                data_packet.checksum = calc_checksum(&data_packet, 13 + bytes);
                
                int idx = seq_num % WINDOW_SIZE;
                win_buff[idx] = data_packet;

                sendto(sockfd, &data_packet, 13 + bytes, 0, (struct sockaddr*)&config.target_addr, config.target_addr_len);
                seq_num++;
            } else {
                ended = true;
            }
        }

        if (ended && base_num == seq_num) {
             break;
        }
        //ACK RECEIVER
        int poll_time = poll(&pfd, 1, 100);

        if (poll_time > 0) {
            rdt_packet recv_packet = {};
            ssize_t bytes_received = recvfrom(sockfd, &recv_packet, sizeof(recv_packet), 0, nullptr, nullptr);

            if (bytes_received >= 13) {
                uint16_t recv_checksum = recv_packet.checksum;
                recv_packet.checksum = 0;
                if (calc_checksum(&recv_packet, bytes_received) == recv_checksum && recv_packet.ack_flag == AckFlags::DATA_ACK) {
                    
                    uint32_t acked_seq = recv_packet.ack_num;
                    if (acked_seq >= base_num) {
                        base_num = acked_seq + 1;
                        client_time = chrono::steady_clock::now();
                    }
                }
            }
        }else if(poll_time == 0){
            for(int i = base_num; i < seq_num; i++) {
                int idx = i % WINDOW_SIZE;
                sendto(sockfd, &win_buff[idx], 13 + win_buff[idx].data_len, 0, (struct sockaddr*)&config.target_addr, config.target_addr_len);
            }

            auto curr_time = chrono::steady_clock::now();
            auto elapsed_time = chrono::duration_cast<chrono::seconds>(curr_time - client_time).count();
            if (elapsed_time >= config.timeout) {
                cerr << "Timeout during data transfer!" << endl; 
                exit(ErrorCodes::TimeoutError);
            }
        } else {
            cerr << "Error: Polling failed." << endl;
            exit(ErrorCodes::NetworkError);
        }

        //END PHASE WITH SERVER
        rdt_packet finish_packet = {};
        finish_packet.seq_num = seq_num;
        finish_packet.ack_num = 0;
        finish_packet.data_len = 0;
        finish_packet.ack_flag = AckFlags::FINAL;

        if (!send_hello(sockfd, finish_packet, AckFlags::FINAL_ACK, config, pfd)) exit(1);

    }
}

int net_setup(udp_trans& config) {
    struct addrinfo hints = {};
    struct addrinfo *dest_addr = nullptr;
    struct addrinfo *ptr = nullptr;

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    if (config.is_server && config.address.empty()) {
        hints.ai_flags = AI_PASSIVE;
    }

    const char* target_ip = config.address.empty() ? nullptr : config.address.c_str();

    if (getaddrinfo(target_ip, config.port_num.c_str(), &hints, &dest_addr) != 0) {
        cerr << "Error getting address info." << endl;
        return -1;
    }

    int sockfd = -1;

    for (ptr = dest_addr; ptr != nullptr; ptr = ptr->ai_next) {
        sockfd = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (sockfd == -1) {
            continue;
        }

        if (config.is_server) {
            if (bind(sockfd, ptr->ai_addr, ptr->ai_addrlen) == -1) {
                close(sockfd);
                sockfd = -1;
                continue; 
            }
        }else {
            memcpy(&config.target_addr, ptr->ai_addr, ptr->ai_addrlen);
            config.target_addr_len = ptr->ai_addrlen;
        }
        break; 
    }

    freeaddrinfo(dest_addr);
    
    if (sockfd == -1) {
        cerr << "Error: Could not create or bind socket." << endl;
        return -1;
    }

    cout << (config.is_server ? "Server" : "Client") << " socket ready on port " << config.port_num << "." << endl;
    return sockfd;
}

int main(int argc, char* argv[]) {
    int opt = 0; 
    bool specified_send_rec = false;

    for (int i = 0; argv[i] != nullptr; i++) {
        if (string(argv[i]) == "--help") {
            getHelp();
        }
    }

    while ((opt = getopt(argc, argv,"hscp:a:i:o:w:")) != -1) {
        switch (opt) {
            case 'h':
                getHelp();
                break;
            case 's':
                if (trans_udp.is_client) {
                    cerr << "Error: Cannot specify both client and server at once." << endl;
                    return ErrorCodes::CliError; 
                }
                specified_send_rec = true;
                trans_udp.is_server = true;
                break;
            case 'c':
                if (trans_udp.is_server) {
                    cerr << "Error: Cannot specify both client and server at once." << endl;
                    return ErrorCodes::CliError; 
                }
                trans_udp.is_client = true;
                specified_send_rec = true;
                break;
            case 'p':
                try {    
                    int port = stoi(optarg);
                    if (port < 0 || port > 65535) {
                        cerr << "Error: Invalid port number." << endl;
                        return ErrorCodes::CliError; 
                    }
                    trans_udp.port_num = to_string(port);
                } catch(...) {
                    cerr << "Error: Port is in an invalid format." << endl;
                    return ErrorCodes::CliError; 
                }
                break;
            case 'a':
                trans_udp.address = optarg;
                break;
            case 'i':
                trans_udp.in_file = optarg;
                break;
            case 'o':
                trans_udp.out_file = optarg;
                break;
            case 'w':
                try {
                    trans_udp.timeout = stoi(optarg);   
                } catch(...) {
                    cerr << "Error: Timeout has to be a number." << endl;
                    return ErrorCodes::CliError; 
                }
                break;
            default:
                cerr << "Error: Non-allowed arguments detected. Use -h for help." << endl;
                return ErrorCodes::CliError; 
        }
    }
    
    if (!specified_send_rec) {
        cerr << "Error: You must specify either -s (server) or -c (client)!" << endl;
        return 1; 
    }
    if (trans_udp.is_client && !trans_udp.out_file.empty()) {
        cerr << "Error: Not a server, you cannot specify an output file (-o)." << endl;
        return 1;
    }

    int sockfd = net_setup(trans_udp);
    if (sockfd < 0) {
        return ErrorCodes::NetworkError; 
    }

    istream* in_stream = nullptr;
    ifstream file_in;
    
    ostream* out_stream = nullptr;
    ofstream file_out;

    if (trans_udp.is_client) {
        if (!trans_udp.in_file.empty()) {
            file_in.open(trans_udp.in_file, ios::binary);
            if (!file_in.is_open()) {
                cerr << "Error: Could not open input file!" << endl;
                close(sockfd);
                return 1;
            }
            in_stream = &file_in;
        } else {
            in_stream = &cin;
        }
        //TODO run socket
    } else if (trans_udp.is_server) {
        if (!trans_udp.out_file.empty()) {
            file_out.open(trans_udp.out_file, ios::binary);
            if (!file_out.is_open()) {
                cerr << "Error: Could not open/create output file!" << endl;
                close(sockfd);
                return ErrorCodes::FileError;
            }
            out_stream = &file_out;
        } else {
            out_stream = &cout;
        }
        //TODO run socket for server
    }

    close(sockfd);
    return 0;
}