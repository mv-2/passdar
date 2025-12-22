run_main:
	clang++ src/main.cpp src/sdrCapture.cpp src/spectrumData.cpp -o main.out -lsdrplay_api -L/usr/local/lib -lfftw3
	./main.out

