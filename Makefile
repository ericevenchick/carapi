OUTPUT = carapi

CC = gcc
CFLAGS = -g

OBJS = server.o

%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(OUTPUT): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

clean:
	-rm *.o $(OUTPUT)

.PHONY: clean
