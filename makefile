TARGET =lib/libnetwork_reactor.a
CXX    =g++
CFLAGS =-g -O2 -Wall -fPIC -Wno-deprecated

INC    =-I./inc
SRC    =./src
OBJS   = $(addsuffix .o, $(basename $(wildcard $(SRC)/*.cpp)))

$(TARGET): $(OBJS)
	mkdir -p lib
	ar cqs $@ $^

%.o: %.cpp
	$(CXX) $(CFLAGS) -c -o $@ $< $(INC) 

.PHONY: clean

clean:
	-rm -f src/*.o $(TARGET)
