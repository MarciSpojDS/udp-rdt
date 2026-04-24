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
} trans_udp;

void getHelp() {
    cout << "IPK-RDT Reliable Data Transfer" << endl;
    cout << "Usage: ./ipk-rdt [-s|-c] -p PORT [options]" << endl;
    exit(0);
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

int net_setup(const udp_trans& config) {
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