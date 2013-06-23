iob: iob.c random.c sync.c psync.c
	gcc iob.c random.c sync.c psync.c -o iob -g -lrt

install: iob
	cp iob /sbin/

clean:
	rm iob
