all: client server dummyProgram.o

client: MBCPhase4.o
	gcc MBCPhase4.o -o client

MBCPhase4.o: MBCPhase4.c
	gcc -c MBCPhase4.c -lpthread

server: MBSPhase4.o
	gcc MBSPhase4.o -o server

MBSPhase4.o: MBSPhase4.c
	gcc -c MBSPhase4.c -lpthread

dummyProgram.o: dummyProgram.c
	gcc -c dummyProgram.c -lpthread -lrt

clean:
	rm *.o client server 
	rm dummyProgram.o