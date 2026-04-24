#include <iostream>

enum ErrorCodes : int {
    CliError = 1,
    NetworkError = 2,
    FileError = 3,
    TimeoutError = 4
};

enum AckFlags: uint8_t {
    HELLO = 1,        
    HELLO_ACK = 2,    

    DATA = 3,         
    DATA_ACK = 4,     
    
    FINAL = 5,         
    FINAL_ACK = 6      
};