run_test:
	clang++ test.c -o test.out -lsdrplay_api -L/usr/local/lib
	./test.out

run_example:
	clang sdrplay_api_example.c -o example.out -lsdrplay_api -L/usr/local/lib
	./example.out $(ARG1) $(ARG2)
