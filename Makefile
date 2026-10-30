CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=c11
LDFLAGS =

TARGET  = sls
SRC     = sls.c
MAN     = sls.1

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGET)

install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/$(TARGET)
	install -d /usr/local/share/man/man1
	install -m 644 $(MAN) /usr/local/share/man/man1/$(MAN)
	-mandb >/dev/null 2>&1 || true
