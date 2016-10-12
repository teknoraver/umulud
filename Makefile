CFLAGS := -Wall -O2 $(shell pkg-config --cflags libusb-1.0)
LDFLAGS := -pthread -lreadline $(shell pkg-config --libs libusb-1.0)

BIN := umulud

HEADERS := $(wildcard *.h)

all: $(BIN)

$(BIN): $(BIN).c $(HEADERS)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

clean::
	rm -f $(BIN)

test:: all
	./$(BIN)
