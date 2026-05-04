CXX = g++
CXXFLAGS = -Wall -Wextra -pedantic -std=c++20 -O2

TARGET = ipk-rdt
SRC = main.cpp

TEST_TARGET = run_tests
TEST_SRC = tests/test.cpp
TEST_SCRIPT = tests/integration.sh

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

NixDevShellName:
	@echo "c"

$(TEST_TARGET): $(TEST_SRC) $(SRC)
	$(CXX) $(CXXFLAGS) -DRUN_TESTS -o $(TEST_TARGET) $(TEST_SRC) $(SRC)

test: all $(TEST_TARGET)
	@echo " Executing Unit Tests"
	./$(TEST_TARGET)
	@echo "Executing Integration Tests"
	@bash $(TEST_SCRIPT)

clean:
	rm -f $(TARGET) $(TEST_TARGET) test_*.bin out_*.bin test_bad.txt