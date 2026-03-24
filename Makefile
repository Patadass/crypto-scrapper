src = $(wildcard ./src/*.c)
CFLAGS = -Wall -lcurl

all:
	$(CC) -o get_data.out $(src) $(CFLAGS)
