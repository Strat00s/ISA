OBJS	= client.o
SOURCE	= client.c
HEADER	= 
OUT	= client
CC	 = g++
FLAGS	 = -g -c -Wall -Wextra -Werror
LFLAGS	 = 

all: $(OBJS)
	$(CC) -g $(OBJS) -o $(OUT) $(LFLAGS)

main.o: main.c
	$(CC) $(FLAGS) main.c -std=c11


clean:
	rm -f $(OBJS) $(OUT)

run: all
	./$(OUT)