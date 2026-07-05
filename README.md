# Passdar

In the effort of learning radar processing and C++ I have attempted to create a
minimal function passive radar using an SDRplay RspDuo with two TV antennas.
The current implementation displays both live spectra from each receiver and
live ambiguity between the incoming signals.

![Ambiguity](images/ambiguity.png "Ambiguity")

![Slices](images/slices.png "Slices")
![Slices](images/spectra.png "Spectra")

## Build & Run

To clone and build the project for a linux machine.

```bash
git clone https://github.com/mv-2/passdar.git
cd passdar
vcpkg install
cmake --preset default-linux
cmake --build --preset default-linux
```

Run by pointing to config file.

```bash
./build/default-linux/passdar cfg/cfg.json
```

On first startup, expect that the window will take a while to show. `FFTW`
is used to calculate the DFT and ambiguity which requires pre-computing
multiple `FFTW` plans using the `FFTW_EXHAUSTIVE` option.

## Dependencies

To build and run, `vcpkg`, `cmake` and `SDRPlay_API v3.15`. This project has not been tested with any other SDRplay API versions at this point in time.
