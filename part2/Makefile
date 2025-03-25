CFLAGS = -Wall -Werror -g
CC = gcc $(CFLAGS)
ARCH = $(shell uname -m)

.PHONY: clean test-setup test clean-tests zip

ifeq ($(ARCH), x86_64)
swish: swish.o string_vector.o swish_funcs.o swish_funcs_provided_x86_64.o
	$(CC) -o $@ $^
else
swish: swish.o string_vector.o swish_funcs.o swish_funcs_provided_aarch64.o
	$(CC) -o $@ $^
endif

swish.o: swish.c
	$(CC) -c $<

string_vector.o: string_vector.c string_vector.h
	$(CC) -c $<

swish_funcs.o: swish_funcs.c swish_funcs.h
	$(CC) -c $<

clean:
	rm -f swish swish.o string_vector.o swish_funcs.o

test-setup:
	@chmod u+x testius
	@rm -f out.txt

ifdef testnum
test: test-setup swish
	./testius test_cases/tests.json -v -n $(testnum)
else
test: swish
	./testius test_cases/tests.json
endif

clean-tests:
	rm -rf test_results out.txt

zip:
	@echo "ERROR: You cannot run 'make zip' from the part2 subdirectory. Change to the main proj3-code directory and run 'make zip' there."
