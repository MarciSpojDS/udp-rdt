#ifndef MAIN_H
#define MAIN_H

#include <iostream>
#include <string>
#include <sys/socket.h>
#include <poll.h>
#include <cstdint>

#define WINDOW_SIZE 25
#define HEADER_SIZE 17

enum ErrorCodes : int {
    CliError = 1,
    NetworkError = 2,
    FileError = 3,
    TimeoutError = 4
};

/* Acknowledgment flags for RDT packets */
enum AckFlags: uint8_t {
    HELLO = 1,        
    HELLO_ACK = 2,    
    DATA = 3,         
    DATA_ACK = 4,     
    FINAL = 5,         
    FINAL_ACK = 6      
};

/* RDT packet structure */
struct __attribute__((packed)) rdt_packet {
    uint32_t connection_id;
    uint32_t seq_num;
    uint32_t ack_num;
    uint16_t data_len;
    uint16_t checksum;
    AckFlags ack_flag;
    char data[1200-HEADER_SIZE]; 
};

/* Configuration informations */
struct Config {
    bool is_server = false;
    bool is_client = false;
    std::string port_num = "";
    std::string address = "";
    std::string in_file = "";
    std::string out_file = "";
    int timeout = 1;
    uint32_t connection_id = 0;
    struct sockaddr_storage target_addr;
    socklen_t target_addr_len = 0;
};

/* Utility functions */
class Utils {
public:
    static void getHelp();
    static uint16_t calc_checksum(const void* data, size_t length);
    static int parse_args(int argc, char* argv[], Config& config);
};

class Server {
private:
    int sockfd;
    Config config;
    struct pollfd pfd;
    std::ostream* out_stream;
    void net_setup();

public:
    Server(const Config& cfg, std::ostream* out);
    ~Server();
    void run();
};

class Client {
private:
    int sockfd;
    Config config;
    struct pollfd pfd;
    std::istream* in_stream;
    void net_setup();
    bool send_hello(rdt_packet& send_pkt, AckFlags expected_flag);

public:
    Client(const Config& cfg, std::istream* in);
    ~Client();
    void run();
    int get_fd() const { return sockfd; }
    socklen_t get_addr_len() const { return config.target_addr_len; }
};

#endif