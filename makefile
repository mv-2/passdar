run_test:
	clang++ test.cpp -o test.out -lsdrplay_api -L/usr/local/lib
	./test.out
