CC = g++
CFLAGS = -Wall -Wno-unused-function
LIBS = -lpthread -lcrypto
DEPS = include/Chord.hpp include/MessageHandler.hpp include/MessageTypes.hpp include/Utils.hpp
OBJS = Chord.o MessageHandler.o
EXECS = sample

all: $(EXECS)

a: clean all

%.o: src/%.cpp $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $< $(LIBS)

sample: src/SampleApp.cpp Chord.o include/Utils.hpp
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

clean:
	rm -rf *.o *~ src/*~ include/*~ include/*.hpp.gch $(EXECS) $(OBJS)
	
cleanall: clean
	rm -rf *_store/
