CC = gcc
CFLAGS = -Wall -Wextra -O2
LDLIBS = -lcrypto

TARGET = dedup
SRC = dedup.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDLIBS)

clean:
	rm -f $(TARGET)

.PHONY: all clean