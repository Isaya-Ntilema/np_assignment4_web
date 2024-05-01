CC = gcc
CC_FLAGS = -w -g
all: serverthread serverfork

serverthread: serverthread.o
	$(CC) -Wall -o serverthread serverthread.o -lpthread 


serverfork:	serverfork.o
	$(CC) -Wall -o serverfork serverfork.o


# serverthread: serverthread.o 
# 	$(CXX) -L./ -Wall -o serverthread serverthread.o -lpthread


clean:
	rm *.o *.a perf_*.txt  tmp.* serverfork serverthread small big
