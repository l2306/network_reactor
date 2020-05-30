CXX=g++
CFLAGS=-g -O2 -Wall -fPIC -Wno-deprecated 

INC=-I../../inc
LIB=-L../../lib -lnetwork_reactor -lpthread -lprotobuf
OBJS = $(addsuffix .o, $(basename $(wildcard *.cc)))

all:
	$(CXX) -o server $(CFLAGS)  server.cpp msg_echo.pb.cc $(INC) $(LIB)
	$(CXX) -o client $(CFLAGS)  client.cpp msg_echo.pb.cc $(INC) $(LIB)

clean:
	-rm -f *.o server client
