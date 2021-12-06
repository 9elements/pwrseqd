
#ifndef _OUTPUTDRIVER_HPP__
#define _OUTPUTDRIVER_HPP__

#include <vector>

using namespace std;

// The output driver applies the signal level to the physical device.
// All output drivers cache the signal level and apply changes when Apply() is
// invoked.
class OutputDriver
{
  public:
    virtual void Apply(void) = 0;
};

class Signal;

// The signal driver pushed signal level updates onto the signals.
// Signals provides a list of signals that are driven by this driver.
// Used for config validation only.
class SignalDriver
{
  public:
    virtual vector<Signal*> Signals(void) = 0;
};

#endif