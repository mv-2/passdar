# Passdar

In the effort of learning radar processing and C++ I have attempted to create a minimal function passive radar using an SDRplay RspDuo with two TV antennas.

## Build & Run

To clone and build the project while cloning external repositories recursively.

```bash
git clone --recursive-submodules https://github.com/mv-2/passdar.git
cd passdar
cmake -B build
cmake --build build
```

Run by pointing to config file.

```bash
./build/passdar cfg/cfg.json
```
