src = $(wildcard ./c_src/*.c)
CFLAGS = -lcurl

all:
	$(CC) -o get_data.out $(src) $(CFLAGS)
