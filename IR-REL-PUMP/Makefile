# Makefile for IR Proximity Sensor (Raspberry Pi 5)
CXX      = g++
CXXFLAGS = -std=c++17 -Wall -O2
LIBS     = -llgpio
TARGET   = ir_pump
SRC      = ir_pump.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $< $(LIBS)

clean:
	rm -f $(TARGET)

install-deps:
	sudo apt update && sudo apt install -y liblgpio-dev

.PHONY: all clean install-deps
