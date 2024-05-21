
#ifndef _OUTPUTDRIVER_HPP__
#define _OUTPUTDRIVER_HPP__

#include <vector>

using namespace std;

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