**Input / Output drivers**:

The input/output driver connect real hardware to the power sequencers
internal logic. 

The following input drivers are implemented:
  - GPIO
  - Item presence (using DBUS xyz.openbmc_project.Inventory.Manager)
  - Voltage regulators enable
  - NULL

The following output drivers are implemented:
  - GPIO
  - LED (using DBUS xyz.openbmc_project.LED.GroupManager)
  - Voltage regulators fault/power good
  - Dummy regulator
  - NULL

**Rules:**

An input driver must fetch the hardware state and call the `SetLevel()` method
of the connected signal. The method can be called asynchronously.

**GPIO driver:**
The GPIO driver requires the GPIO pin to be present in the current system.
A failure of finding the pin will terminate the application.

**Null driver:**
The null driver doesn't need real hardware and should be used for testing only.

**Voltage regulators:**

The driver exposes the following signals:
 - `<Name>_On`
 - `<Name>_Enabled`
 - `<Name>_Fault`
 - `<Name>_PowerGood`
 - `<Name>_IsDummy`

It reads the `<Name>_On` signal and drives the following signals:
 - `<Name>_Enabled`
 - `<Name>_Fault`
 - `<Name>_PowerGood`

Where:

> PowerGood = `<Name>_Enabled` && !`<Name>_Fault`
> Fault = `<Name>_Enabled` && "Internal Fault detect"
> No real regulator was found `<Name>_IsDummy` = true

Writing 0 to `<Name>_On` clears the following signals:
 - `<Name>_Enabled`
 - `<Name>_Fault`
 - `<Name>_PowerGood`

