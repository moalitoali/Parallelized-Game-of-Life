CC = gcc
CFLAGS = -O3 -funroll-loops
LDFLAGS = -lpthread
RM = /bin/rm -f

gameOfLife: gameOfLife.c 
	$(CC) $(CFLAGS) -o gameOfLife gameOfLife.c $(LDFLAGS)

clean:
	$(RM) ./gameOfLife