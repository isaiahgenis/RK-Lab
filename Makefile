ANSWER=1
CFLAGS=-g

all: rkmatch bloom_test

rkmatch: rkmatch.o bloom.o
	gcc -m32 -lm $< bloom.o -o $@ -lrt

bloom_test : bloom_test.o bloom.o
	gcc -m32 -lm $< bloom.o -o $@

%.o : %.c
	gcc -m32 $(CFLAGS) -DANSWER=$(ANSWER) -c ${<}

handin:
	tar -cvf handin.tar *

clean :
	rm -f *.o rkmatch bloom_test
