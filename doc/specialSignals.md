# Special signals

The pwrseqd defines special signals to interact with the user.
The following signals are set on user basis to turn on/off the
chassis and the host:

- STATE_REQ_HOST_ON
- STATE_REQ_CHASSIS_ON

Those are connected to DBus methods 'RequestedHostTransition' and 'RequestedPowerTransition'.

The following special signal are defined within pwrseqd to notify
the user about the power state and that POST is done:

- STATE_POST_DONE
- ACPI_STATE_IS_G3
- ACPI_STATE_IS_S5
- ACPI_STATE_IS_S3
- ACPI_STATE_IS_S0

Those signals are connected to DBus properties 'CurrentHostState',
'CurrentPowerState', 'BootProgress' and 'OperatingSystemState'.