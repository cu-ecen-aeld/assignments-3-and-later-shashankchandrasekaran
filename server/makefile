CC = $(CROSS_COMPILE)gcc
COMPFLAG = -Wall -Werror			
.PHONY: all
all: aesdsocket
writer :
	$(CC) $(COMPFLAG) -c aesdsocket.c -o aesdsocket.o
	$(CC) $(COMPFLAG) -o aesdsocket aesdsocket.o	
	
.PHONY: clean
clean:
	rm -f *.o aesdsocket					
