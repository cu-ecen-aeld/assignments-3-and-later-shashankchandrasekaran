CC = $(CROSS_COMPILE)gcc
COMPFLAG = -Wall -Werror			
.PHONY: all
all: writer
writer :
	$(CC) $(COMPFLAG) -c writer.c -o writer.o
	$(CC) $(COMPFLAG) -o writer writer.o	
	
.PHONY: clean
clean:
	rm -f *.o writer					
