#ifndef FAN_H
#define FAN_H

enum class FanMode
{
  Auto,
  Manual
};

void initializeFan();
void updateFan();
void setFanMode(FanMode mode);
FanMode getFanMode();
bool isFanOn();
bool setManualFan(bool on);

#endif // FAN_H
