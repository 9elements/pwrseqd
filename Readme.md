# Power sequencing daemon (pwrseqd)

The *pwrseqd* is a replacement for the CPLD found on modern server
mainboards. It has to be run on the BMC capable of controlling
all power VRs, monitoring all rails, controlling GPIOs and doing thermal
management.
This software is meant to be used on [OpenBmc](https://github.com/openbmc/openbmc).

Detailed implementation can be found in [HighLevelOverview](doc/HighLevelOverview).

# How to build

```bash
mkdir build
cd build
cmake ..
make
```

# External dependencies

- libyaml-cpp
- libgpiod
- libgpiodcxx
- libpthread
- libbsdbusplus
- libphosphor-logging

# Config files

The program needs one or multiple configuration file in YAML syntax to
operate. The configuration describes the power sequencing, which is very
similar to the verilog/VHDL code used to compiled into a CPLD bitstream.

Configuration files are merged on program startup.
For details see [Configuration file layout](doc/config.md).

## License
```
     Apache License
Version 2.0, January 2004
```