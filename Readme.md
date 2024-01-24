# Power sequencing daemon (pwrseqd)

The *pwrseqd* is a replacement for the CPLD found on modern server
mainboards. It has to be run on the BMC capable of controlling
all power VRs, monitoring all rails, controlling GPIOs and doing thermal
management.
This software is meant to be used on [OpenBmc](https://github.com/openbmc/openbmc).

Detailed implementation can be found in [HighLevelOverview](doc/HighLevelOverview.md).

The whole application is designed around Linux kernel features and requires the
following options to be enabled:
```
CONFIG_REGULATOR=y
CONFIG_REGULATOR_NETLINK_EVENTS=y
CONFIG_REGULATOR_USERSPACE_CONSUMER=y
CONFIG_GPIO=y
CONFIG_GPIO_CDEV=y
```

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

Also see special signals [Predefined signals](doc/specialSignals.md).
## License
```
     Apache License
Version 2.0, January 2004
```