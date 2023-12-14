# Writing config files

Config files are the essential part and most critical part of power sequencing.
There are lot's of runtime checks that protect from common mistakes and enforces
a valid config.

The configuration is written in YAML. You can use multiple config files located
in the same folder. However there must be no duplicated section within *the same*
config file.

## Config file layout
Every configuration file can contain one of the following:

- inputs
- outputs
- power_sequencer
- regulators
- states
- floating_signals
- immutables

**inputs**

Inputs describe an input driver. Input drivers operate asynchronously.

**outputs**

Outputs describe an output driver. Output driver operate synchronously.

**power_sequencer**

Describe a Logic unit similar to a LUT in a CPLD.

**regulators**

Regulator is a driver that uses regulators found in `/sys/class/regulator`. 
In order to enable/disable regulators the *reg-userspace-consumer* kernel driver
must be present.

At the moment only enabling/disabling regulators is supported. The output voltage
can't be set.

**floating_signals**

Floating signals is a whitelist of signals that are not connected. Usually unconnected
signals are an error, but as regulators automatically spawn signals, this array was added.

**immutables**

A list of signals with constant level.


## Outputs

Outputs is an array of ConfigOutput structures that describe which GPIO to
use, how to configure it and which internal signal it is connected to.
In case `type` is "gpio" a GPIO is driven with the state of the signal given
by `signal_name`.
In case `type` is "led" a LED is driven using the D-Bus service
`xyz.openbmc_project.LED.GroupManager`.

Every ConfigOutput structure has the following keys:

- `name`: "GPIO_name" String. Name as reported by gpioinfo.
- `type`: One of: "gpio", "led", "null"
- `signal_name`: String. The name of the signal that is
  driving this gpio
- `active_low`: boolean. If true the pin will be configured as Active Low
- `open_drain`: boolean. If true the pin will be configured as Open Drain
- `open_source`: boolean. If true the pin will be configured as Open Drain
- `pullup`: boolean. If true the pin will be configured as Pull UP
- `pulldown`: boolean. If true the pin will be configured as Pull Down
- `disablebias`: boolean. If true the pin will be configured to have no bias
- `gpio_chip_name`: String. Name of the GPIO chip (optional)
- `description`: String. Ignored by code.


### Example

```yaml
outputs:
  - name: "RST_PCH_RSMRST_R_N"
    type: "gpio"
    signal_name: "RSMRST"
    active_low: true
    open_drain: true
    description: >
      All PCH primary power rails are stable for 10 msec.
      Either assert after SLP_SUS_N or 100msec after DSW_PWROK assertion.
```

## Inputs

Inputs is an array of ConfigInput structures that describe which GPIO to
use, how to configure it and which internal signal it is connected to.
In case `type` is "gpio" the signal given by `signal_name` is set by the GPIO
level.
In case `type` is "dbus_presence" the signal is driven by the D-Bus property
`Present` of the D-Bus service `xyz.openbmc_project.Inventory.Manager`.

Every ConfigInput structure has the following keys:

- `name`: "GPIO_name" String. Name as reported by gpioinfo.
- `type`: One of: "gpio", "dbus_presence", "null"
- `signal_name`: String. The name of the signal that is
  driving this gpio
- `active_low`: boolean. If true the pin will be configured as Active Low
- `open_drain`: boolean. If true the pin will be configured as Open Drain
- `open_source`: boolean. If true the pin will be configured as Open Drain
- `pullup`: boolean. If true the pin will be configured as Pull UP
- `pulldown`: boolean. If true the pin will be configured as Pull Down
- `disablebias`: boolean. If true the pin will be configured to have no bias
- `gpio_chip_name`: String. Name of the GPIO chip (optional)
- `description`: String. Ignored by code.
- `gate_input`: boolean. Adds an <name>_On signal to the input driver. When <name>_On
                         is low the input is gated and `signal_name` isn't read from the
                         GPIO any more.
                         When high the signal `signal_name`  is driven from actual GPIO level.
- `gated_idle_high`: boolean. When gated the signal reads as high.
- `gated_idle_low`: boolean. When gated the signal reads as low.
- `gated_output_od_low`: boolean. When gated the pin is set to output and driven low.

### Example

```yaml
inputs:
  - name: "H_LVT3_CPLD0_THERMTRIP_N"
    signal_name: "H_LVT3_CPLD0_THERMTRIP"
    type: "gpio"
    active_low: true
    description: >
      THERMTRIP_N is set on over tempertature condition or internal FIVR fault.
    gate_input: true
    gated_idle_high: true

power_sequencer:
  - H_LVT3_CPLD0_THERMTRIP_N_Enable_Unit:
      in:
        and:
          - name: "pvccio_cpu0_On"
          - name: "pvccio_cpu0_PowerGood"
      out:
        name: "H_LVT3_CPLD0_THERMTRIP_N_On"
```

## Regulators

regulators is an array of ConfigRegulator structures that describe which regulator
to use. The regulator must have only one userspace-consumer that exposes control
to user-space.

Every ConfigRegulator structure has the following keys:
- `name`: String. Name of the regulator.
- `description`: String. Ignored by code.
- `timeout_usec`: integer. Time in usec to poll for regulator to change state.

### Example 1
```yaml
regulators:
  - name: "PVPP_GHJ"
```

## Immutable

immutables is an array of ConfigImmutable structures that described fixed signals.

Every ConfigImmutable structure has the following keys:
- `signal_name`: String. The name of the signal that is immutable.
- `value`: boolean. Value of signal.

## Logic

power_sequencer is an array of ConfigLogic structures that described logic blocks.

Every ConfigLogic structure has the following keys:
- `in`: Map. Describes the inputs of the logic block.
  - `and`: array of ConfigLogicInput (optional)
  - `or`: array of ConfigLogicInput (optional)
  - `and_then_or`: boolean. Output of AND is input of OR. If false Output of OR is input of AND.
  - `invert_first_gate`: boolean. Invert first gate logic output before feeding to next logic gate.

- `out`: ConfigLogicOutput struct. Describes the output of the logic block.
- `delay_usec`: int. Fixed delay of output driver.

### Example 1

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

### Example 2

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

## LogicInput

ConfigLogicInput structure describes inputs of logic blocks.

Every ConfigLogicInput structure has the following keys:
- `name`: String. Name of the signal to read.
- `invert`: boolean. Invert the signal state.
- `input_stable_usec`: int. Wait for the input signal to not change at least specified microseconds.

## LogicOutput

ConfigLogicOutput structure describes the output of logic blocks.

Every ConfigLogicOutput structure has the following keys:
- `name`: String. Name of the signal to read.
- `active_low`: boolean. Invert the signal state.
