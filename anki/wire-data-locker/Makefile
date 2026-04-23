all: user-data-locker

user-data-locker: user-data-locker.c
	${CC} $(CFLAGS) $(LDFLAGS) -o $@ $^

clean:
	rm user-data-locker
	