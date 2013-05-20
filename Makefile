OUTPUT = carapi

CC = gcc
CFLAGS = -g
LIBS = -lpthread

OBJS = server.o canstore.o can.o canstore_parser.o

%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(OUTPUT): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

clean:
	-rm *.o $(OUTPUT)

.PHONY: clean
