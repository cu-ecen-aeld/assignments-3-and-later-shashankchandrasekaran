CC = $(CROSS_COMPILE)gcc
COMPFLAG = -Wall -Werror			
.PHONY: all
all: aesdsocket
aesdsocket :
	$(CC) $(COMPFLAG) -c aesdsocket.c -o aesdsocket.o
	$(CC) $(COMPFLAG) -o aesdsocket aesdsocket.o -lpthread -lrt
	
.PHONY: clean
clean:
	rm -f *.o aesdsocket					
