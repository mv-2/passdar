run_test:
	clang test.c -o test.out -lsdrplay_api -L/usr/local/lib
	./test.out

run_example:
	clang fourier.c sdrplay_api_example.c -o example.out -lsdrplay_api -L/usr/local/lib -lm
	./example.out $(ARG1) $(ARG2)

CONFIG_PATH ?= config.json

run_live:
	clang fourier.c live_output.c -o live.out -lsdrplay_api -L/usr/local/lib -lm -lcjson
	./live.out "$(CONFIG_PATH)"
