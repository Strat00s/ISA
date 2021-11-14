OBJS	= client.o
SOURCE	= client.c
OUT	= client
CC	 = g++
FLAGS	 = -g -c -Wall -Wextra -Werror

all: $(OBJS)
	$(CC) -g $(OBJS) -o $(OUT) $(LFLAGS)

main.o: main.c
	$(CC) $(FLAGS) main.c


clean:
	rm -f $(OBJS) $(OUT)