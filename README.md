### Warning
#### this code contain OTA feature that will use memory flash, keep binary under 240 KB
- Safe Zone (Under 240 KB / < 46%): The update sequence will pass validation checks and copy cleanly.
- Danger Zone (Above 240 KB / > 46%): PlatformIO will build it without errors because it technically fits inside the raw physical 512 KB storage of the chip, but your OTA upload step will fail or freeze the board midway through because InternalStorage.open() will run out of space in the second bank partition.

### Fan control

The fan starts in **auto** mode and runs until both temperature sensors have
valid readings. In auto mode, it runs whenever a jammer is commanded on or a
jammer state sensor reports on. Otherwise, it turns on when either temperature
exceeds 40 °C and turns off only when both temperatures are below 30 °C. Between
these thresholds it retains its previous state. If either temperature sensor is
missing or invalid, the fan runs as a precaution. Thresholds can be changed in
`src/config.h`.

Use `POST /fan/mode` with `{"mode":"auto"}` or `{"mode":"manual"}` to change
modes. Switching to auto immediately applies the automatic rules; switching to
manual retains the current fan state. In manual mode, use `POST /fan` with
`{"is_on":true}` or `{"is_on":false}`; jammer and temperature changes do not
override manual control. `POST /fan` returns HTTP 409 in auto mode. `GET /fan`
returns the current mode and fan state. The mode resets to auto after restart.
See `test/test.http` for example requests.

Run the fan control host tests with:

```sh
c++ -std=c++11 -Wall -Wextra -Werror -I test/host/stubs -I src test/host/fan_test.cpp src/fan.cpp -o /tmp/djammer-fan-test && /tmp/djammer-fan-test
```

### Firmware version and serial number

Set `FIRMWARE_VERSION` and the device's `SERIAL_NUMBER` in `src/config.h` before
building and uploading. The serial number defaults to `"UNSET"` until you set
it. `GET /version` returns the compiled-in values and board name, for example
`{"status":"success","version":"1.1.0","board":"<board name>","sn":"<serial number>"}`.
Changing the configuration requires rebuilding and uploading the firmware.


## yang belum
- alamat ip simpan di eeprom