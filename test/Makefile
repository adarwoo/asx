
# Compiler
CXX = g++

# Compiler flags
CXXFLAGS = -std=c++11 -I/usr/local/include -std=c++23
CXXFLAGS += -I../include

# Linker flags
LDFLAGS = -L/usr/local/lib -lgtest -lgtest_main -pthread

# Source files
SRCS = test_bitstore.cpp test_debouncer.cpp

# Object files
OBJS = $(SRCS:.cpp=.o)

# Executable name
TARGET = runTests

# Default target
all: $(TARGET)

# Link the executable
$(TARGET): $(OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)

# Compile source files into object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up build files
clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean