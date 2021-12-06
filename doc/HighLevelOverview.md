**The x86 power sequencer**:

The x86 power sequencer (PSD) operates like a software CPLD.
The program binary reads in a configuration file describing the logic blocks
that build a fixed function pipeline. GPIOs / voltages / faults are then used
to feed the fixed function pipeline and react accordingly.

The goal is to replace the CPLD found on modern x86 servers.

- The x86 power state machine will consume one or more unit configuration files in YAML or JSON format.
  The unit configuration file describes:

   - GPIO inputs
   - GPIO outputs
   - voltage regulator enable
   - voltage regulator faults
   - Monitor ICs fault outputs
   - Watchdogs
   - FAN controllers faults
   - Logic blocks

- The x86 power state machine will consume one constrain config file in YAML or JSON format.
  The constrains define minimum delays, maximum delays or order for rising signals and falling signals.
  The x86 power state machine can use the constrain file to check if the "units" described above pass the x86 vendor requirements for certain timings.

- Every unit either drives a signal, reads a signal or is doing both.
- A unit can be of the following type:

  - Input
  - Output
  - Logic
- Units are connected by signals. The sum of all signals reflect the current
  state of the state machine.

- A logic unit connects one or multiple inputs to a single output signal.
  - Outputs can be *activeLow (bool) flag*, which inverses the internal logic level 
  - Outputs can be delayed by n useconds *delay (int) flag*
  - Outputs have their state defined by the LUT and the LUT's input signals.

- A logic unit has on or multiple inputs.
   - Inputs can be negated by *invert (bool) flag*
   - Inputs can be routed to AND or OR gate
   - Inputs are stable for at least n useconds *stable (int) flag*
   - The order of AND and OR gate can be set by *andThenOr (bool) flag*.
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

- The x86 power state sequencer will use interrupts to detect GPIO state changes.
  On GPIO state changes, power state change (S5->S0, S0->S5, ...) or timeouts the internal state machine is run.
  The state machine is run in a loop until no more signal changes.
  The state machine iterates over all units and execute their Run() method.

