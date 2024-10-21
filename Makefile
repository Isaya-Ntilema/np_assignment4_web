# Makefile

CXX = g++
CXXFLAGS = -std=c++11 -pthread

all: serverthread serverfork

serverthread: serverthread.cpp
	$(CXX) $(CXXFLAGS) -o serverthread serverthread.cpp

serverfork: serverfork.cpp
	$(CXX) $(CXXFLAGS) -o serverfork serverfork.cpp

clean:
	rm -f serverthread serverfork

