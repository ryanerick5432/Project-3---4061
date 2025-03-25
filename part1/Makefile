CFLAGS = -Wall -Werror -g
CC = gcc $(CFLAGS)

.PHONY: clean test-setup test clean-tests zip

par_letter_counts: par_letter_counts.c
	$(CC) -o $@ $^

clean:
	rm -f par_letter_counts

test-setup:
	@chmod u+x testius

ifdef testnum
test: test-setup par_letter_counts
	./testius test_cases/tests.json -v -n $(testnum)
else
test: test-setup par_letter_counts
	./testius test_cases/tests.json
endif

clean-tests:
	rm -rf test_results

zip:
	@echo "ERROR: You cannot run 'make zip' from the part1 subdirectory. Change to the main proj3-code directory and run 'make zip' there."
