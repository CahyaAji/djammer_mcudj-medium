### Warning
#### this code contain OTA feature that will use memory flash, keep binary under 240 KB
- Safe Zone (Under 240 KB / < 46%): The update sequence will pass validation checks and copy cleanly.
- Danger Zone (Above 240 KB / > 46%): PlatformIO will build it without errors because it technically fits inside the raw physical 512 KB storage of the chip, but your OTA upload step will fail or freeze the board midway through because InternalStorage.open() will run out of space in the second bank partition.

### Fan control

The fan starts off after a restart. A valid `POST /jammer` command with
`"is_on": true`, or a `POST /jammers` command with any jammer set to `true`,
turns the fan on, even if the command repeats an existing state. Commands that
turn jammers off do not turn the fan off.

Use `POST /fan` with `{"is_on": true}` or `{"is_on": false}` to control the fan
manually, including turning it off while a jammer is active. The next jammer-on
command turns it on again. Temperature, sensor errors, and jammer sensor changes
do not control the fan; the operator must manage cooling. See `test/test.http`
for example requests.


## yang belum
- alamat ip simpan di eeprom
- fan turn on/off by temperature (auto)