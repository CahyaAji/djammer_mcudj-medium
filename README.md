### Warning
#### this code contain OTA feature that will use memory flash, keep binary under 240 KB
- Safe Zone (Under 240 KB / < 46%): The update sequence will pass validation checks and copy cleanly.
- Danger Zone (Above 240 KB / > 46%): PlatformIO will build it without errors because it technically fits inside the raw physical 512 KB storage of the chip, but your OTA upload step will fail or freeze the board midway through because InternalStorage.open() will run out of space in the second bank partition.