CC = g++
CFLAGS = -Wall -Wno-unused-function
LIBS = -lpthread -lcrypto -lgmp -lgmpxx
DEPS = include/Chord.hpp include/MessageHandler.hpp include/MessageTypes.hpp include/Utils.hpp
OBJS = Chord.o MessageHandler.o
EXECS = sample

all: $(EXECS)

a: clean all

MessageHandler.o: src/MessageHandler.cpp include/MessageTypes.hpp include/MessageHandler.hpp
	$(CC) $(CFLAGS) -c -o $@ $< $(LIBS)

Chord.o: src/Chord.cpp include/Chord.hpp include/Utils.hpp MessageHandler.o
	$(CC) $(CFLAGS) -c -o $@ $< $(LIBS)

sample: src/SampleApp.cpp Chord.o MessageHandler.o include/Utils.hpp
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)
	
s1: sample
	./sample -c 48693 -p 31627

s2: sample
	./sample -c 48693 -p 9332 -j 128.10.3.51

clean:
	rm -rf *.o *~ src/*~ include/*~ include/*.hpp.gch $(EXECS) $(OBJS)
	
cleanall: clean
	rm -rf *_store/
