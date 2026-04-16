#include <iostream>
#include <unistd.h>
#include <string>
#include "main.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>


using namespace std;

struct {
    bool is_server = false;
    bool is_client = false;
    int port_num; 
    string address = "";
    string in_file = "";
    string out_file = "";
    int timeout = 1;//seconds
} trans_udp;


void getHelp(){
    cout << "Help" << endl;
    exit(0);
}

int main(int argc, char* argv[]) {

    int opt = 0; 
    bool specified_send_rec = false;

    for (int i = 0; argv[i] != '\0'; i++)
    {
        if (string(argv[i]) == "--help")
        {
            getHelp();
        }
    }
    
    while ((opt = getopt(argc, argv,"hscp:a:i:o:w:")) != -1)
    {
        switch (opt)
        {
            case 'h':
                getHelp();
                return 0;
                break;
            case 's':
                if (trans_udp.is_client)
                {
                    cerr << "Error you can not specify both client and server at once." << endl;
                    return ErrorCodes::CliError; 
                }
                specified_send_rec = true;
                trans_udp.is_server = true;
                break;
            case 'c':
                if (trans_udp.is_server)
                {
                    cerr << "Error you can not specify both client and server at once." << endl;
                    return ErrorCodes::CliError; 
                }
                trans_udp.is_client = true;
                specified_send_rec = true;
                break;
            case 'p':
                try
                {    
                    trans_udp.port_num = stoi(optarg);
                    if (trans_udp.port_num < 0 || trans_udp.port_num > 65535)
                    {
                        cerr << "This port number " << trans_udp.port_num << " does not exist!" << endl;
                        return ErrorCodes::CliError; 
                    }
                }
                catch(const std::exception& e)
                {
                    cerr << "Port is in an invalid format." << endl;
                    cerr << "Exception: " << &e << endl;
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
                if (!trans_udp.is_server)
                    {
                        cerr << "Not a server, you can not specify an output file." << endl;
                        return ErrorCodes::CliError; 
                    }
                    break;
            case 'w':
                try
                {
                    trans_udp.timeout = stoi(optarg);   
                }
                catch(const std::exception& e)
                {
                    cerr << "Timeout has to be a number." << endl;
                    cerr << "Exception: " << &e << endl;
                    return ErrorCodes::CliError; 
                }
                break;
            default:
                cerr << "Non-allowed arguments detected check -h or --help for manual." << endl;
                return ErrorCodes::CliError; 
        }
    }
    
    if (!specified_send_rec)
    {
        cerr << "Exited with code: 1"<< endl;
        cerr << "The user did not specify -s (send) or -r (receive)!" << endl;
        return ErrorCodes::CliError; 
    }
    

    return 0;
}