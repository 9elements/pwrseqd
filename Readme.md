# Power sequencing daemon (pwrseqd)

The *pwrseqd* is a replacement for the CPLD found on modern server
mainboards. It has to be run on the BMC capable of controlling
all power VRs, monitoring all rails, controlling GPIOs and doing thermal
management.

Detailed implementation can be found in [HighLevelOverview](doc/HighLevelOverview).

# How to build

```bash
mkdir build
cd build
cmake ..
```

# External dependencies

libyaml-cpp
libgpiod
libgpiodcxx
libpthread
libbsdbusplus
libphosphor-logging

# Config files

The program needs one or multiple configuration file in YAML syntax to
operate. The configuration describes the power sequencing, which is very
similar to the verilog/VHDL code used to compiled into a CPLD bitstream.

Configuration files are merged on program startup.

## Config file layout
Every configuration file can contain one of the following:

- inputs
- outputs
- power_sequencer
- regulators
- switch
- states
- floating_signals
- immutables

**inputs**

Inputs describe an input driver. Input drivers operate asynchronously.

Example:

```yaml
  - name: "H_LVT3_CPLD1_THERMTRIP_N"
    signal_name: "H_LVT3_CPLD1_THERMTRIP"
    type: "gpio"
    active_low: true
    description: >
      THERMTRIP_N is set on over tempertature condition or internal FIVR fault.
```

In this example a `GPIO` type input driver is instantiated that drives
the internal signal `H_LVT3_CPLD1_THERMTRIP`. It reads the information from a
gpio named `H_LVT3_CPLD1_THERMTRIP_N` and due to the `active_low` it inverts
the signal level.

**outputs**

Outputs describe an output driver. Output driver operate synchronously.

Example:

```yaml
  - name: "H_LVT3_CPU3_PROCHOT_N"
    type: "gpio"
    signal_name: "H_LVT3_CPU3_PROCHOT"
    active_low: true
    description: "CPU3 is hot and throttling. Open Drain."
```

In this example a `GPIO` type output driver is instantiated that reads
the internal signal `H_LVT3_CPU3_PROCHOT`. It sets the signal level on a
gpio named `H_LVT3_CPU3_PROCHOT_N` and due to the `active_low` it inverts
the signal level.


**power_sequencer**

Describe a Logic unit similar to a LUT in a CPLD.

Example 1:

```yaml
  - CPU_THERMTRIP_Unit:
      in:
        or:
          - name: "CPU0_THERMTRIP"
          - name: "CPU1_THERMTRIP"
          - name: "CPU2_THERMTRIP"
          - name: "CPU3_THERMTRIP"
      out:
        name: "THRMTRIP"
```

Reads from signals `CPU0_THERMTRIP`, `CPU1_THERMTRIP`, `CPU2_THERMTRIP`
and `CPU3_THERMTRIP` and does a logical OR. It drives the result onto
the signal `THRMTRIP`. Every change on one of the input signal reevaluates
the logical expresssion.

Example 2:
```yaml
  - RSMRST_Unit:  # When high then AUX power is on and system is in S5, else G3
      in:
        and:
          - name: "P1V05_PCH_AUX_PowerGood"
            input_stable_usec: 10000  # LBG: min 10 msec
          - name: "P1V8_PCH_AUX_PowerGood"
            input_stable_usec: 10000  # LBG: min 10 msec
      out:
        name: "RSMRST_N"
```

Reads from signals `P1V05_PCH_AUX_PowerGood` and `P1V8_PCH_AUX_PowerGood`
waiting for signal level to be stable for at least 10 milli seconds.
Once both are stable it drives the RSMRST_N to the same signal level.

**regulators**

Regulator is a driver that uses regulators found in `/sys/class/regulator`. 
In order to enable/disable regulators the *reg-userspace-consumer* kernel driver
must be present.

Example:
```yaml
  - name: "PVPP_GHJ"
```

At the moment only enabling/disabling regulators is supported. The output voltage
can't be set.


**floating_signals**

Floating signals is a whitelist of signals that are not connected. Usually unconnected
signals are an error, but as regulators automatically spawn signals, this array was added.

**states**

A list of supported ACPI states and initial ACPI state.

```yaml
  - name: "G3"
    initial: true
```

**immutables**

A list of signals with constant level.
