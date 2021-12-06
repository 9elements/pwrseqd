**Fixed function StateMachine**:

The fixed function statemachine consists of logic blocks connected together
by signals. The current state is determined by the sum of all signals.

Assumptions:
- All signals are driven asynchronously. Interrupt handlers and timers might
  set the state at any time. Threads might poll for data and push it periodically.
- Information flows from inputs to outputs. Signal level changes are only
  propaged in the direction of output. Data is not requested from inputs.

Requirement:
- GPIO outputs are changed periodically at once (no glitches).
- Signal propagation must be as short as possible.

Implementation:

A single StateMachine takes care of aggregating the signal state.
Every signal has a "dirty" bit. It is set by the data provider on signal level
change.  The "dirty" bit causes the StateMachine to be invoked as fast as possible.

For every "dirty" signal the StateMachine walks all listeners. That is
all signals that depend on the "dirty" signal needs to be re-evaluated.
While re-evaluating such, signals might change level and will also set the "dirty" bit.
When re-evaluting the signal dependency tree the dirty bit is cleared.

Having an oscillating loop in the fixed function pipeline might prevent the
StateMachine from entering a steady state. **FIXME**

The StateMachine must keep running until no more "dirty" signals are present.
In this state all outputs must be updated at once.



