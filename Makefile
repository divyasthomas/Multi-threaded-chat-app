CC=gcc
CFLAGS= -Wall -g -std=c99
DEPS = list.h 
OBJ = lets-talk.o list.o 


all: lets-talk 

%.o:	%.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)


lets-talk:	$(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) -pthread 
	
valgrind:
	valgrind --leak-check=full ./lets-talk 3000 localhost 3001
	
clean:
	rm *.o $(objects)  lets-talk 
	
	

