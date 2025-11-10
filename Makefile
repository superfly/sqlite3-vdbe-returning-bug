CC = gcc

# Uncomment ONE of the following lines to choose SQLite source:
# Use custom SQLite amalgamation:
#SQLITE3_PATH = ../corrosion/local-deps/libsqlite3-sys/sqlite3
# Use system SQLite (default):
SQLITE3_PATH =

# Build configuration based on SQLITE3_PATH
ifdef SQLITE3_PATH
    CFLAGS = -Wall -g -I$(SQLITE3_PATH)
    SQLITE3_SRC = $(SQLITE3_PATH)/sqlite3.c
else
    CFLAGS = -Wall -g
    SQLITE3_SRC =
    LDFLAGS = -lsqlite3
endif

TARGET = bug_poc
SRC = bug_poc.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(SQLITE3_SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
