# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -Wpedantic -O2 -g

# Directories
SRC_DIR = src
OBJ_DIR = obj
TEST_DIR = tests

# Files
SRCS = $(wildcard $(SRC_DIR)/*.cpp)
OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRCS))
TARGET = ipk-rdt

# Phony targets to prevent conflicts with files of the same name
.PHONY: all clean test NixDevShellName

# Default target required to build the executable
all: 
	$(TARGET)

# --- STRICT NIX ENVIRONMENT REQUIREMENT ---
# The @ symbol prevents the command itself from echoing; it only prints the output.
NixDevShellName:
	@echo "c"

# Linking the final executable in the repository root
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Compiling source files into object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Create object directory if it doesn't exist
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# --- AUTOMATED TESTS REQUIREMENT ---
# Assuming you will write a bash script to run your automated tests
test: $(TARGET)
	@echo "Running automated tests..."
	@chmod +x $(TEST_DIR)/run_tests.sh
	./$(TEST_DIR)/run_tests.sh

# Clean up build artifacts
clean:
	rm -rf $(OBJ_DIR) $(TARGET)