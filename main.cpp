#include "main.h"
#include <fstream>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <netdb.h>
#include <chrono>

using namespace std;

/* --- DisplaysHelp Message --- */
void Utils::getHelp() {
    cout << "IPK-RDT: Reliable Data Transfer over UDP\n\n"
         << "Usage:\n"
         << "  Server: ./ipk-rdt -s -p PORT [-a ADDRESS] [-o OUTPUT] [-w TIMEOUT]\n"
         << "  Client: ./ipk-rdt -c -a HOST -p PORT [-i INPUT] [-w TIMEOUT]\n\n"
         << "Required Modes (Exactly one MUST be specified):\n"
         << "  -s            Start the receiving side of the application (Server).\n"
         << "  -c            Start the sending side of the application (Client).\n\n"
         << "Arguments:\n"
         << "  -p PORT       Specifies the UDP port number.\n"
         << "  -a ADDRESS    [Server] Specifies the local bind address. If omitted, listens on all interfaces.\n"
         << "  -a HOST       [Client] Specifies the destination hostname or IPv4/IPv6 address.\n"
         << "  -i INPUT      [Client] Specifies the input file to send. If omitted or '-', reads from stdin.\n"
         << "  -o OUTPUT     [Server] Specifies the output file to create/overwrite. If omitted or '-', writes to stdout.\n"
         << "  -w TIMEOUT    Specifies a positive timeout in whole seconds. Default is 1.\n"
         << "  -h, --help    Writes these usage instructions to stdout and terminates.\n";
    exit(0);
}

/* --- Checksum Calculation by RFC 1071 standards, used to check packet integrity, byte calculation --- */
uint16_t Utils::calc_checksum(const void* data, size_t length) {
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

/* --- Argument Parsing, Initializes the configuration, checks for errors in CLI arguments --- */
int Utils::parse_args(int argc, char* argv[], Config& trans_udp) {
    int opt = 0; 
    bool specified = false;

    for (int i = 0; argv[i] != nullptr; i++) {
        if (string(argv[i]) == "--help"){
            getHelp();
        }
    }

    while ((opt = getopt(argc, argv,"hscp:a:i:o:w:")) != -1) {
        switch (opt) {
            case 'h': 
                getHelp(); 
                break;
            case 's':
                if (trans_udp.is_client){return ErrorCodes::CliError;} 
                specified = true; 
                trans_udp.is_server = true; 
                break;
            case 'c':
                if (trans_udp.is_server){ return ErrorCodes::CliError;}
                trans_udp.is_client = true; specified = true; break;
            case 'p':
                try {    
                    int port = stoi(optarg);
                    if (port < 0 || port > 65535){ return ErrorCodes::CliError;}
                    trans_udp.port_num = to_string(port);
                } catch(...) { return ErrorCodes::CliError; }
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
                } 
                catch(...){ 
                    return ErrorCodes::CliError; 
                }
                break;
            default: 
                return ErrorCodes::CliError;
                break; 
        }
    }

    if (!specified || trans_udp.port_num.empty()) return ErrorCodes::CliError;
    if (trans_udp.is_client && trans_udp.address.empty()) return ErrorCodes::CliError;
    if (trans_udp.is_client && !trans_udp.out_file.empty()) return ErrorCodes::CliError;
    
    return 0;
}

/* --- Server Constructor --- */
Server::Server(const Config& cfg, ostream* out) : sockfd(-1), config(cfg), out_stream(out) {
    net_setup();
    pfd.fd = sockfd;
    pfd.events = POLLIN;
}

/* --- Server Destructor --- */
Server::~Server() {
    if (sockfd != -1) {
        close(sockfd);
    }
}

/*--- Initializes configuration for server, tests the address information, binds packet to port ---*/
void Server::net_setup() {
    struct addrinfo hints = {}, 
    *dest_addr = nullptr;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    if (config.address.empty()){
        hints.ai_flags = AI_PASSIVE;
    } 

    const char* target_ip = config.address.empty() ? nullptr : config.address.c_str();
    
    if (getaddrinfo(target_ip, config.port_num.c_str(), &hints, &dest_addr) != 0) {
        cerr << "Error getting address info" << endl;
        exit(ErrorCodes::NetworkError);
    }

    for (; dest_addr != nullptr; dest_addr = dest_addr->ai_next) {
        sockfd = socket(dest_addr->ai_family, dest_addr->ai_socktype, dest_addr->ai_protocol);
        
        if (sockfd == -1){
            continue;
        }
        
        if (bind(sockfd, dest_addr->ai_addr, dest_addr->ai_addrlen) == -1) {
            close(sockfd); sockfd = -1; continue; 
        }
        break; 
    }

    freeaddrinfo(dest_addr);
    
    if (sockfd == -1) {
        cerr << "Error: Could not bind" << endl;
        exit(ErrorCodes::NetworkError);
    }
    cerr << "Server socket ready on port:" << config.port_num << endl;
}

/* --- Connection that communicates with the client, handshake finish and start, information receive, send --- */
void Server::run() {
    auto start_time = chrono::steady_clock::now();
    struct sockaddr_storage client_addr;
    socklen_t client_len = sizeof(client_addr);
    rdt_packet recv_packet = {};
    uint32_t connection_id = 0;

    while (true) {   
        int poll_time = poll(&pfd, 1, 1000);

        if (poll_time > 0) {
            ssize_t bytes_rec_received = recvfrom(sockfd, &recv_packet, sizeof(recv_packet), 0, (struct sockaddr*)&client_addr, &client_len);
            
            if (bytes_rec_received > 0 && bytes_rec_received >= HEADER_SIZE) {
                uint16_t received_checksum = recv_packet.checksum;
                recv_packet.checksum = 0;
               
                if (Utils::calc_checksum(&recv_packet, sizeof(recv_packet)) == received_checksum) {
                    if (recv_packet.ack_flag == AckFlags::HELLO) {
                        connection_id = recv_packet.connection_id;
                        rdt_packet ack_pkt = {};
                        
                        ack_pkt.connection_id = connection_id;
                        ack_pkt.ack_flag = AckFlags::HELLO_ACK;
                        ack_pkt.checksum = Utils::calc_checksum(&ack_pkt, sizeof(ack_pkt));
                        sendto(sockfd, &ack_pkt, sizeof(ack_pkt), 0,(struct sockaddr*)&client_addr, client_len);
                        
                        start_time = std::chrono::steady_clock::now();
                        break;
                    }
                }
            }
        } else if (poll_time == 0) {
            if (chrono::duration_cast<chrono::seconds>(chrono::steady_clock::now() - start_time).count() >= config.timeout) {
                cerr << "Error: Handshake Timeout" << endl; 
                exit(ErrorCodes::TimeoutError); 
            }
        } else { exit(ErrorCodes::NetworkError); }
    }
    
    uint32_t expext_seq = 1;
    rdt_packet last_packet = {};
    last_packet.ack_flag = AckFlags::DATA_ACK;
    last_packet.connection_id = connection_id;
    last_packet.checksum = Utils::calc_checksum(&last_packet, HEADER_SIZE);
    auto server_time = chrono::steady_clock::now();
    
    while (true) {
        int poll_timer = poll(&pfd, 1, 1000); 
        
        if (poll_timer > 0) {
            ssize_t received = recvfrom(sockfd, &recv_packet, sizeof(recv_packet), 0, (struct sockaddr*)&client_addr, &client_len);
            
            if (received > 0 && received >= HEADER_SIZE) {
                uint16_t recv_cs = recv_packet.checksum;
                recv_packet.checksum = 0;
                if (recv_packet.connection_id != connection_id) {
                    continue;
                }

                if (Utils::calc_checksum(&recv_packet, received) == recv_cs) {
                    if (recv_packet.ack_flag == AckFlags::DATA && recv_packet.seq_num == expext_seq) {
                        out_stream->write(recv_packet.data, recv_packet.data_len);
                        
                        last_packet.ack_num = expext_seq;
                        last_packet.checksum = 0;
                        last_packet.checksum = Utils::calc_checksum(&last_packet, HEADER_SIZE);
                        
                        sendto(sockfd, &last_packet, HEADER_SIZE, 0, (struct sockaddr*)&client_addr, client_len);
                        
                        expext_seq++;
                        server_time = chrono::steady_clock::now();
                    } else if (recv_packet.ack_flag == AckFlags::DATA) {
                        sendto(sockfd, &last_packet, HEADER_SIZE, 0, (struct sockaddr*)&client_addr, client_len);
                    } else if (recv_packet.ack_flag == AckFlags::FINAL) {
                        rdt_packet fin_ack = {};
                        fin_ack.ack_flag = AckFlags::FINAL_ACK;
                        fin_ack.checksum = Utils::calc_checksum(&fin_ack, HEADER_SIZE);
                        
                        sendto(sockfd, &fin_ack, HEADER_SIZE, 0, (struct sockaddr*)&client_addr, client_len);
                        
                        cerr << "FIN_ACK sent. Server shutting down safely." << endl;
                        break; 
                    }
                }
            }
        } else if (poll_timer == 0) {
            if (chrono::duration_cast<chrono::seconds>(chrono::steady_clock::now() - server_time).count() >= config.timeout) {
                cerr << "Error: Timeout ERROR" << endl; exit(ErrorCodes::TimeoutError);
            }
        } else { 
            exit(ErrorCodes::NetworkError); 
        }
    }
}

/* --- Client Constructor --- */
Client::Client(const Config& cfg, istream* in) : sockfd(-1), config(cfg), in_stream(in) {
    net_setup();
    pfd.fd = sockfd;
    pfd.events = POLLIN;
}

/* --- Client Destructor --- */
Client::~Client() {
    if (sockfd != -1) {
        close(sockfd);
    }
}

/* --- Initializes client information, binds packet to port, validates destination --- */
void Client::net_setup() {
    struct addrinfo hints = {}, *dest_addr = nullptr, *ptr = nullptr;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    const char* target_ip = config.address.empty() ? nullptr : config.address.c_str();
    
    if (getaddrinfo(target_ip, config.port_num.c_str(), &hints, &dest_addr) != 0) {
        cerr << "Error getting address info." << endl;
        exit(ErrorCodes::NetworkError);
    }

    for (ptr = dest_addr; ptr != nullptr; ptr = ptr->ai_next) {
        sockfd = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        
        if (sockfd == -1){ 
            continue;
        }
        
        memcpy(&config.target_addr, ptr->ai_addr, ptr->ai_addrlen);
        config.target_addr_len = ptr->ai_addrlen;
        break; 
    }

    freeaddrinfo(dest_addr);
    
    if (sockfd == -1) {
        cerr << "Error: Could not create or bind socket." << endl;
        exit(ErrorCodes::NetworkError);
    }
    cerr << "Client socket ready on port " << config.port_num << endl;
}

/* --- Hello handshake and Finish Handshake --- */
bool Client::send_hello(rdt_packet& send_pkt, AckFlags expected_flag) {
    auto start_time = chrono::steady_clock::now();
    while (true) {
        send_pkt.checksum = 0;
        send_pkt.checksum = Utils::calc_checksum(&send_pkt, HEADER_SIZE + send_pkt.data_len);
        sendto(sockfd, &send_pkt, HEADER_SIZE + send_pkt.data_len, 0, (struct sockaddr*)&config.target_addr, config.target_addr_len);
        
        int poll_time = poll(&pfd, 1, 100);
        
        if (poll_time > 0) {
            rdt_packet recv_pkt = {};
            ssize_t bytes_rec = recvfrom(sockfd, &recv_pkt, sizeof(recv_pkt), 0, nullptr, nullptr);
            
            if (bytes_rec > 0 && bytes_rec >= HEADER_SIZE) {
                uint16_t cs = recv_pkt.checksum;
                recv_pkt.checksum = 0;
                if (Utils::calc_checksum(&recv_pkt, bytes_rec) == cs && recv_pkt.ack_flag == expected_flag) {
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

/* --- Client Communication, handshakes, recieve, send --- */
void Client::run() {
    rdt_packet hello_packet = {};
    hello_packet.ack_flag = AckFlags::HELLO;
    hello_packet.connection_id = config.connection_id;

    uint32_t seq_num = 1;
    uint32_t base_num = 1;

    if (!send_hello(hello_packet, AckFlags::HELLO_ACK)) {
        exit(ErrorCodes::TimeoutError);
    }

    rdt_packet win_buff[WINDOW_SIZE] = {}; 
    bool ended = false;
    auto client_time = chrono::steady_clock::now();

    while(true) {
        while (seq_num < base_num + WINDOW_SIZE && !ended) {
            rdt_packet data_packet = {};
            in_stream->read(data_packet.data, sizeof(data_packet.data));
            streamsize bytes_rec = in_stream->gcount();

            if (bytes_rec > 0) {
                data_packet.seq_num = seq_num; 
                data_packet.connection_id = config.connection_id;
                data_packet.data_len = bytes_rec;
                data_packet.ack_flag = AckFlags::DATA;
                data_packet.checksum = Utils::calc_checksum(&data_packet, HEADER_SIZE + bytes_rec);
                
                win_buff[seq_num % WINDOW_SIZE] = data_packet;
                
                sendto(sockfd, &data_packet, HEADER_SIZE + bytes_rec, 0, (struct sockaddr*)&config.target_addr, config.target_addr_len);
                seq_num++;
            } else { 
                ended = true; 
            }
        }

        if (ended && base_num == seq_num) {
            break;
        }

        int poll_time = poll(&pfd, 1, 100);
        
        if (poll_time > 0) {
            rdt_packet recv_packet = {};
            ssize_t bytes_rec_received = recvfrom(sockfd, &recv_packet, sizeof(recv_packet), 0, nullptr, nullptr);
            
            if (bytes_rec_received > 0 && bytes_rec_received >= HEADER_SIZE) {
                uint16_t recv_checksum = recv_packet.checksum;
                recv_packet.checksum = 0;
                
                if (Utils::calc_checksum(&recv_packet, bytes_rec_received) == recv_checksum && recv_packet.ack_flag == AckFlags::DATA_ACK) {
                    if (recv_packet.ack_num >= base_num) {
                        base_num = recv_packet.ack_num + 1;
                        client_time = chrono::steady_clock::now();
                    }
                }
            }
        } else if(poll_time == 0) {
            for(uint32_t i = base_num; i < seq_num; i++) {
                int idx = i % WINDOW_SIZE;
                sendto(sockfd, &win_buff[idx], HEADER_SIZE + win_buff[idx].data_len, 0, (struct sockaddr*)&config.target_addr, config.target_addr_len);
            }
            if (chrono::duration_cast<chrono::seconds>(chrono::steady_clock::now() - client_time).count() >= config.timeout) {
                cerr << "Error: Timeout during data transfer" << endl; 
                exit(ErrorCodes::TimeoutError);
            }
        } else { 
            exit(ErrorCodes::NetworkError); 
        }
    }

    rdt_packet finish_packet = {};
    finish_packet.seq_num = seq_num;
    finish_packet.ack_flag = AckFlags::FINAL;
    finish_packet.connection_id = config.connection_id;
    
    if (!send_hello(finish_packet, AckFlags::FINAL_ACK)) {
        exit(ErrorCodes::TimeoutError);
    }
}

#ifndef RUN_TESTS
int main(int argc, char* argv[]) {
    Config config;
    int parse_num = Utils::parse_args(argc, argv, config);
    if (parse_num != 0) {
        return parse_num;
    }

    if (config.is_client) {
        srand(time(NULL));
        config.connection_id = rand();
        ifstream file_in;
        istream* in_stream = &cin;
        
        if (!config.in_file.empty()) {
            file_in.open(config.in_file, ios::binary);
            if (!file_in.is_open()) {
                cerr << "Error: Could not open input file" << endl;
                return ErrorCodes::FileError;
            }
            in_stream = &file_in;
        }
        
        Client client(config, in_stream); 
        client.run();                        
        
    } else if (config.is_server) {
        ofstream file_out;
        ostream* out_stream = &cout;
        
        if (!config.out_file.empty()) {
            file_out.open(config.out_file, ios::binary);
            if (!file_out.is_open()) {
                cerr << "Error: Could not open output file" << endl;
                return ErrorCodes::FileError;
            }
            out_stream = &file_out;
        }
        
        Server server(config, out_stream); 
        server.run();                         
    }

    return 0; 
}
#endif