CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wpedantic -Werror -O2

wish: wish.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

test: wish
	./test-wish.sh -c

clean:
	rm -rf wish tests-out

.PHONY: test clean
