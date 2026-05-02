#!/bin/bash

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo "==================================================="
echo "  Starting COMPLEX INTEGRATION TESTS (IPK-RDT)     "
echo "==================================================="

rm -f test_*.bin out_*.bin test_bad.txt

# Test files
echo "Generating test data..."
dd if=/dev/urandom of=test_small.bin bs=1K count=50 2>/dev/null    
dd if=/dev/urandom of=test_large.bin bs=1M count=5 2>/dev/null     
touch test_empty.bin                                               
echo "THIS IS A BAD FILE FROM A HACKER" > test_bad.txt


# TEST 1: Timeout Test
echo -e "\n[TEST 1] Timeout Test (Server must shut down after 2s)"
./ipk-rdt -s -p 9001 -w 2 > /dev/null 2>&1 &
SERVER_PID=$!
wait $SERVER_PID
if [ $? -ne 0 ]; then
    echo -e "${GREEN}[OK] Server correctly dropped due to Timeout.${NC}"
else
    echo -e "${RED}[FAIL] Server exited with code 0!${NC}"
fi

# TEST 2: Alien Packet (Votrelec)
echo -e "\n[TEST 2] Alien Packet (ID Protection / Multiplexing)"
./ipk-rdt -s -p 9002 -o out_large.bin -w 5 > /dev/null 2>&1 &
SERVER_PID=$!
sleep 0.5

./ipk-rdt -c -a 127.0.0.1 -p 9002 -i test_large.bin -w 5 > /dev/null 2>&1 &
CLIENT_A_PID=$!
sleep 0.5

./ipk-rdt -c -a 127.0.0.1 -p 9002 -i test_bad.txt -w 2 > /dev/null 2>&1
if [ $? -ne 0 ]; then
    echo -e "${GREEN}[OK] Alien (Client B) was ignored.${NC}"
else
    echo -e "${RED}[FAIL] Alien passed through!${NC}"
fi

wait $CLIENT_A_PID
wait $SERVER_PID

if cmp -s test_large.bin out_large.bin; then
    echo -e "${GREEN}[OK] Received 5MB file is uncorrupted.${NC}"
else
    echo -e "${RED}[FAIL] File was corrupted by Alien!${NC}"
fi

# TEST 3: Stdin to File (Piping)
echo -e "\n[TEST 3] Stdin to File (cat file | client)"
./ipk-rdt -s -p 9003 -o out_stdin.bin -w 3 > /dev/null 2>&1 &
SERVER_PID=$!
sleep 0.5

cat test_small.bin | ./ipk-rdt -c -a 127.0.0.1 -p 9003 -w 3 > /dev/null 2>&1
wait $SERVER_PID

if cmp -s test_small.bin out_stdin.bin; then
    echo -e "${GREEN}[OK] Piping to client works flawlessly.${NC}"
else
    echo -e "${RED}[FAIL] Error reading from Stdin.${NC}"
fi

# TEST 4: File to Stdout 
echo -e "\n[TEST 4] File to Stdout (server > file)"
# Server vypisuje do stdout, tak to presmerujeme do bash suboru
./ipk-rdt -s -p 9004 -w 3 > out_stdout.bin 2>/dev/null &
SERVER_PID=$!
sleep 0.5

./ipk-rdt -c -a 127.0.0.1 -p 9004 -i test_small.bin -w 3 > /dev/null 2>&1
wait $SERVER_PID

if cmp -s test_small.bin out_stdout.bin; then
    echo -e "${GREEN}[OK] Server Stdout output works flawlessly.${NC}"
else
    echo -e "${RED}[FAIL] Error writing to Stdout.${NC}"
fi

# TEST 5: Empty File (0 bytes)
echo -e "\n[TEST 5] Empty file (0 bytes)"
./ipk-rdt -s -p 9005 -o out_empty.bin -w 3 > /dev/null 2>&1 &
SERVER_PID=$!
sleep 0.5

./ipk-rdt -c -a 127.0.0.1 -p 9005 -i test_empty.bin -w 3 > /dev/null 2>&1
wait $SERVER_PID

if [ ! -s out_empty.bin ] && [ -f out_empty.bin ]; then
    echo -e "${GREEN}[OK] Empty file successfully transferred without crash.${NC}"
else
    echo -e "${RED}[FAIL] Program failed to handle empty file.${NC}"
fi

# TEST 6: Duplaction and Loss
echo -e "\n[TEST 6] Network Hell (10% Loss, 10% Duplication)"
echo -e "${YELLOW}Note: Script might ask for password (sudo) to simulate bad network.${NC}"

sudo tc qdisc add dev lo root netem loss 10% duplicate 10%
./ipk-rdt -s -p 9006 -o out_net.bin -w 10 > /dev/null 2>&1 &
SERVER_PID=$!
sleep 0.5

./ipk-rdt -c -a 127.0.0.1 -p 9006 -i test_small.bin -w 10 > /dev/null 2>&1
wait $SERVER_PID
sudo tc qdisc del dev lo root netem

if cmp -s test_small.bin out_net.bin; then
    echo -e "${GREEN}[OK] Retransmission works! File passed uncorrupted even with packet loss.${NC}"
else
    echo -e "${RED}[FAIL] Transfer failed. Lost packets were not resent or file was corrupted.${NC}"
fi

rm -f test_*.bin out_*.bin test_bad.txt
echo -e "\n==================================================="
echo -e "${GREEN}ALL INTEGRATION TESTS COMPLETED.${NC}"
echo "==================================================="