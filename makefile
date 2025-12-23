# Compiler
CC = clang++

# Compiler flags
CFLAGS = -O3 -pedantic

# target naming
TARGET = passdar
TARGET_DIR = target

# config file
CONFIG_PATH = cfg/cfg.json

# source files
SRCS = src/main.cpp src/sdrCapture.cpp src/spectrumData.cpp src/cfgInterface.cpp

# linked librarys
LIBS = -lsdrplay_api -L/usr/local/lib -lfftw3 -ljsoncpp -Iexternal/imgui -Iexternal/implot

all:
	make compile
	make $(TARGET)

compile:
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET_DIR)/$(TARGET) $(LIBS)

$(TARGET):
	./$(TARGET_DIR)/$(TARGET) $(CONFIG_PATH)

clean:
	rm $(TARGET_DIR)/$(TARGET)
