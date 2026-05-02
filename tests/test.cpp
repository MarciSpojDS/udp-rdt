#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <cassert>
#include <unistd.h>
#include <cstdint>
#include "../main.h"

using namespace std;

int parse_helper(vector<string> args, Config& config) {
    optind = 0;
    vector<char*> cstrings;
    cstrings.push_back(const_cast<char*>("./ipk-rdt"));
    for (auto& s : args) {
        cstrings.push_back(const_cast<char*>(s.c_str()));
    }
    return Utils::parse_args(cstrings.size(), cstrings.data(), config);
}

void run_network_tests() {
    cout << "\nStarting Unit Tests for Network Mapping..." << endl;

    {
        Config config = {};
        config.is_client = true;
        config.port_num = "9000";
        config.address = "127.0.0.1";
        
        Client node(config, &cin);
        assert(node.get_fd() > 0); 
        assert(node.get_addr_len() > 0); 
        cout << "[OK] Network Test 1" << endl;
    }

    {
        Config config = {};
        config.is_client = true;
        config.port_num = "9000";
        config.address = "::1"; 
        
        Client node(config, &cin);
        assert(node.get_fd() > 0);
        assert(node.get_addr_len() > 0);
        cout << "[OK] Network Test 2" << endl;
    }

    {
        Config config = {};
        config.is_client = true;
        config.port_num = "9000";
        config.address = "localhost"; 
        
        Client node(config, &cin);
        assert(node.get_fd() > 0);
        assert(node.get_addr_len() > 0);
        cout << "[OK] Network Test 3" << endl;
    }
}

void run_checksum_tests() {
    cout << "\nStarting Unit Tests for Checksum..." << endl;

    {
        const char* data = "AABB"; 
        uint16_t cs = Utils::calc_checksum(data, 4);
        assert(cs != 0); 
        cout << "[OK] Checksum Test 1" << endl;
    }

    {
        const char* data = "AABBC"; 
        uint16_t cs = Utils::calc_checksum(data, 5);
        assert(cs != 0);
        cout << "[OK] Checksum Test 2" << endl;
    }

    {
        char data1[] = "Hello World!";
        char data2[] = "Hello World?"; 
        
        uint16_t cs1 = Utils::calc_checksum(data1, strlen(data1));
        uint16_t cs2 = Utils::calc_checksum(data2, strlen(data2));
        
        assert(cs1 != cs2); 
        cout << "[OK] Checksum Test 3" << endl;
    }

    {
        char data1[] = "Testovaci string 123";
        char data2[] = "Testovaci string 123";
        
        uint16_t cs1 = Utils::calc_checksum(data1, strlen(data1));
        uint16_t cs2 = Utils::calc_checksum(data2, strlen(data2));
        
        assert(cs1 == cs2); 
        cout << "[OK] Checksum Test 4" << endl;
    }

    {
        rdt_packet pkt = {};
        pkt.seq_num = 1;
        pkt.connection_id = 9999;
        pkt.ack_flag = AckFlags::DATA; 
        pkt.data_len = 5;
        memcpy(pkt.data, "DATA!", 5);

        pkt.checksum = 0; 
        uint16_t cs = Utils::calc_checksum(&pkt, HEADER_SIZE + pkt.data_len); 
        assert(cs != 0);
        cout << "[OK] Checksum Test 5" << endl;
    }
}

void run_all_tests() {
    cout << "Starting Unit Tests for CLI Arguments..." << endl;

    {
        Config config = {};
        int result = parse_helper({"-s", "-p", "9000"}, config);
        assert(result == 0); 
        assert(config.is_server == true);
        assert(config.is_client == false);
        assert(config.port_num == "9000");
        assert(config.timeout == 1); 
        cout << "[OK] Test 1" << endl;
    }

    {
        Config config = {};
        int result = parse_helper({"-s", "-p", "8080", "-a", "127.0.0.1", "-o", "vystup.bin", "-w", "5"}, config);
        assert(result == 0);
        assert(config.is_server == true);
        assert(config.port_num == "8080");
        assert(config.address == "127.0.0.1");
        assert(config.out_file == "vystup.bin");
        assert(config.timeout == 5);
        cout << "[OK] Test 2" << endl;
    }

    {
        Config config = {};
        int result = parse_helper({"-c", "-p", "5555", "-a", "localhost"}, config);
        assert(result == 0);
        assert(config.is_client == true);
        assert(config.is_server == false);
        assert(config.port_num == "5555");
        assert(config.address == "localhost");
        cout << "[OK] Test 3" << endl;
    }

    {
        Config config = {};
        int result = parse_helper({"-s", "-c", "-p", "9000"}, config);
        assert(result != 0); 
        cout << "[OK] Test 4" << endl;
    }

    {
        Config config = {};
        int result = parse_helper({"-p", "9000"}, config);
        assert(result != 0); 
        cout << "[OK] Test 5" << endl;
    }

    {
        Config config = {};
        int result = parse_helper({"-s", "-p", "abc"}, config);
        assert(result != 0);
        cout << "[OK] Test 6" << endl;
    }

    {
        Config config = {};
        int result = parse_helper({"-s", "-p", "70000"}, config);
        assert(result != 0);
        cout << "[OK] Test 7" << endl;
    }

    {
        Config config = {};
        int result = parse_helper({"-c", "-p", "9000", "-a", "127.0.0.1", "-o", "zly_napad.txt"}, config);
        assert(result != 0);
        cout << "[OK] Test 8" << endl;
    }

    {
        Config config = {};
        int result = parse_helper({"-s", "-p", "9000", "-w", "nekonecno"}, config);
        assert(result != 0);
        cout << "[OK] Test 9" << endl;
    }

    {
        Config config = {};
        int result = parse_helper({"-s"}, config);
        assert(result != 0); 
        cout << "[OK] Test 10" << endl;
    }

    {
        Config config = {};
        int result = parse_helper({"-c", "-p", "9000", "-a", ""}, config);
        assert(result != 0); 
        cout << "[OK] Test 11" << endl;
    }
}

int main() {
    run_all_tests();
    run_network_tests();
    run_checksum_tests();
    
    cout << "All the tests passed successfully!" << endl;
    return 0;
}