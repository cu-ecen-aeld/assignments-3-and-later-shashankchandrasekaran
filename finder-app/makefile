CCOMP = $(CROSS_COMPILE)gcc	
COMPFLAG = -Wall -Werror			
.PHONY: all
all: writer
writer :
	$(CCOMP) $(COMPFLAG) -c writer.c -o writer.o
	$(CCOMP) $(COMPFLAG) -o writer writer.o	
	
.PHONY: clean
clean:
	rm -f *.o writer					
