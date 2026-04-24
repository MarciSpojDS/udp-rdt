# Variables
CXX = g++
CXXFLAGS = -Wall -Wextra -pedantic -std=c++20 -O2
TARGET = ipk-rdt
SRC = main.cpp

# Default target when you just type 'make'
all: $(TARGET)

# Rule to build the executable
$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

# Clean target to remove the compiled files (run 'make clean')
clean:
	rm -f $(TARGET)