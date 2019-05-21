main:
	g++ -o bs branchsearch.cpp -lncurses

install: main
	cp bs /bin/