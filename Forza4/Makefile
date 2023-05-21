CFLAGS=-Wall -std=gnu99
INCLUDES=-I./inc

F4SERVER_SRCS=src/errExit.c src/semaphore.c src/shared_memory.c src/message_queue.c src/F4server.c  
F4CLIENT_SRCS=src/errExit.c src/semaphore.c src/shared_memory.c src/message_queue.c src/F4client.c 

F4SERVER_OBJS=$(F4SERVER_SRCS:.c=.o)
F4CLIENT_OBJS=$(F4CLIENT_SRCS:.c=.o)

all: F4server F4client

F4server: $(F4SERVER_OBJS)
	@echo "Making executable: "$@
	@$(CC) $^ -o $@

F4client: $(F4CLIENT_OBJS)
	@echo "Making executable: "$@
	@$(CC) $^ -o $@

.c.o:
	@echo "Compiling: "$<
	@$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

.PHONY: clean

clean:
	@rm -f src/*.o F4client F4server
	@echo "Removed object files and executables..."