CC     = gcc
CFLAGS = -Wall

SRC    = src/download.c
INC    = include
TARGET = download

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -I$(INC) -o $@ $^

clean:
	rm -f $(TARGET)
