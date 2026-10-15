CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Iinclude
LIBS = -lncurses

SRC = src/main.cpp \
      src/process_reader.cpp \
      src/system_reader.cpp \
      src/tui.cpp

TARGET = monitor

all:
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $(LIBS)

run: all
	./$(TARGET)

clean:
	rm -f $(TARGET)