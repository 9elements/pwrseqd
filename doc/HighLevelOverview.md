**The power sequencer**:

The pwrseqd operates like a software CPLD.
The program binary reads in a configuration file describing the logic blocks
that build a fixed function pipeline. GPIOs / voltages / faults are then used
to feed the fixed function pipeline and react accordingly.

The goal is to replace the CPLD found on modern x86 servers.

- The pwrseqd state machine will consume one or more unit configuration files in YAML format.
  The unit configuration file describes:

   - GPIO inputs
   - GPIO outputs
   - inventory presence (detected over DBUS)
   - voltage regulator enable
   - voltage regulator faults
   - monitor ICs fault outputs
   - logic blocks
   - signals (connecting each of the above)

- Every unit either drives a signal, reads a signal or is doing both.
- A unit can be of the following type:

  - Input
  - Output
  - Logic

- Units are connected by signals. The sum of all signals reflect the current
  state of the state machine.

- A logic unit connects one or multiple inputs to a single output signal.
  - Outputs can be *activeLow (bool) flag*, which inverses the internal logic level 
  - Outputs can be delayed by `n` microseconds *delay (int) flag*

- A logic unit has on or multiple inputs.
   - Inputs can be negated by *invert (bool) flag*
   - Inputs can be routed to `AND` or `OR` gate
   - Inputs are stable for at least n useconds *stable (int) flag*
   - The order of `AND` and `OR` gate can be set by *andThenOr (bool) flag*.
   - The first gate output can be inverted *InvertFirstGate (bool) flag*.

andThenOr: true

```
in_a---|--------|
in_b---| AND    |--|--------|
in_c--o|--------|  | OR     |-----(Active Low)----- Output
                   |        |
            in_d- -|--------|
                      
```

andThenOr: false

```
in_a---|--------|
in_b---| OR     |--|--------|
in_c--o|--------|  | AND    |-----(Active Low)----- Output
                   |        |
            in_d- -|--------|
                        
```

- The pwrseqd state sequencer will use interrupts to detect GPIO state changes.
- The pwrseqd state sequencer will use sysfs notify to detect regulator state changes.
- On external state change or timeouts the internal state machine is run.
  The state machine is run in a loop until no more signal changes.
  When this happens the new output is driven to the GPIOs marked as output/regulator
  enables and dbus properties.
