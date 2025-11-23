run_test:
	clang src/test.c -o test.out -lsdrplay_api -L/usr/local/lib
	./test.out

run_example:
	clang src/fourier.c src/sdrplay_api_example.c -o example.out -lsdrplay_api -L/usr/local/lib -lm
	./example.out A ms

run_live:
	clang src/fourier.c src/sdr_utils.c src/live_output.c -o live.out -lsdrplay_api -L/usr/local/lib -lm -lcjson
	./live.out config.json
