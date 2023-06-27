# STM32 USB-HFU bootloader
This project is a bootloader to enable updates of a user application using a USB mass storage device plugged into the USB host port of the STM32 microcontroller.

Facts about this project :
- It is designed for applications that expose the USB host port to the user, and to work without any button : plug the mass storage device with the update file in the root directory then power up the MCU and wait up to 10 seconds.
- It may run on many STM32 devices with USB host capabilities. My own targets were F105 and F205 devices ; it compiles with F401 target (with minor changes related to FatFs version) however it it untested.
- It works with FAT formatted USB mass storage devices thanks to FATFS by ChaN (http://elm-chan.org/fsw/ff/00index_e.html). The update file is read from the root filesystem and can use long filename.
- Before the update, it checks that the flash content differs to the file to prevent wearing the memory. Only the required flash pages are erased.
- It can check the CRC32 of the update file
- If filesystem is writeable, it can dump the existing flash content to the USB storage before the update. It can also write a summary log file.
- The bootloader requires a delay at power up before starting the user application. This is because it needs to wait for the mass storage to power up and identify on the USB bus. If the delay is too short, or the mass storage is too slow to identify, the user application may start before it is detected by the bootloader.

# Disclaimer
This is a hobbyist project, it comes with no warranty and no support.

I publish my work under the CC-BY-NC-SA license.

If you find it useful and want to reward my hard work : I am always looking for Amiga/Amstrad CPC/Commodore(...) hardware to repair and hack, please contact me.

# Configure the project for your target
Here are some steps for a STM32CubeIDE project :
- Configure the device with :
  - USB\_OTG as "Host only",
  - USB\_HOST as "Mass Storage Host Class",
  - FATFS as "USB Disk" with the following defines :
    - FS\_READONLY can be enabled when FS\_LOCK is set to 0 and dump/log features are not needed,
    - FS\_MINIMIZE can be set to 3 when readonly is activated,
    - USE\_LFN may be enabled with static working buffer,
  - CRC has to be activated to check the CRC before update
  - High speed clock is required for some devices to allow 48MHz clock and USB operation,
  - Debug is also useful.
- Copy command.c and command.h inside the project,
- Edit the generated main.c and usb\_host.c to insert the "USER CODE" from the files in this repository,
- In command.h, set the defines to your needs :
  - filenames, use 8+3 chars if LFN is disabled,
  - application address to check write protection. The application start address must be offset for at least the bootloader bin size and aligned to flash page/sector boundary. See below.
  - CRC, Dump, Log activation
- In command.c, you may also check that the getFlashSector() function returns the correct sector number for your device.

# Required modifications to user application project
As a bootloader, it takes the place at the beginning of the STM32 MCU flash memory. This requires to move the user application at a higher address on a flash page/sector boundary before compilation. This address has to be defined in the bootloader before compilation so it must be decided beforehand and set both in the bootloader and the user application.

Here are some steps for a STM32CubeIDE application :
- Edit the _FLASH.ld file at the root of the project
  - In "Memories definition", change the FLASH ORIGIN to the new application address and remove this same offset size from the LENGTH.
- Edit system\_stm32xxxx.c
  - uncomment #define USER\_VECT\_TAB\_ADDRESS,
  - change the VECT\_TAB\_OFFSET value to the one you get from APPLICATION\_ADDRESS - FLASH_BASE.
- In the menu "Project"/"Properties" then "C/C++ Build"/"Settings", check "Convert to binary file".

After you build the project Release, you get the .bin file that is used by the bootloader. Make sure it has the exact same name as specified in the bootloader when you place it inside the root directory of your USB mass storage device.

Info : if you need to debug your application while keeping the bootloader, then you need to specify the "vector table address" inside the "Run/Debug configuration". Go to the "STM32 C/C++ Application" panel "Startup" tab, and "Start Address" "Runtime Option".

# CRC32 check
The bootloader can calculate the CRC32 of the updated firmware and, when it is appended at the end of the file, check that the file has not been altered. This "special" CRC32 can be calculated on a computer with the provided "stm-crc32" command line tool in this repository.
```
gcc stm-crc32.c -o stm-crc32
stm-crc32 [-o dest_file] source_file
```
If a destination file name is provided, the content of the source file is copied there and the CRC32 written at the end of the file, creating a firmware file that should be accepted by the CRC32 check of the bootloader. Otherwise the CRC32 of the source file is printed to the standard output.

I spent hours trying to get the same CRC32 value from my STM32 MCU and other tools installed on my computer where I generate my firmwares. With no luck. CRC32 is no standard and every tool does it in a way different from the other, calculating different CRC32 values. This "stm-crc32" tool is written around the source code provided by ST to show the actual performance of the CRC peripheral of its MCUs, inside AN4187 https://www.st.com/en/embedded-software/stsw-stm32an4187.html.

When the firmware file is modified to include the CRC32, I suggest to use a file extension different than the .bin generated by the build environment to prevent confusion. This extension must be set in the bootloader FLASH\_FILENAME.

# Logging
When activated, the bootloader produces a log in the USB mass storage device root directory.
```
Cycles=006a7f5f Check=08008000 CRC=2cf47c8d Flashed=1
```
- Cycles : the number of iterations of the main loop before the USB mass storage has been mounted. This value can help to calibrate the WAIT_CYCLES value
- Check : the result of the verifications done before flashing
  - 0= file check successful and Flash memory is the same as file content
  - 1= file open error
  - 2= file size not multiple of 32 bits (obviously not a firmware)
  - 4= file read error
  - 8= vector table in file mismatch (obviously not a firmware)
  - 16= read number of bytes and file size mismatch (obviously a read error)
  - 32= CRC32 from file read error
  - 64= CRC32 in file mismatch (file is probably corrupted)
  - >= Flash address of first difference (the file checks are OK and the content in flash is different than in file)
- CRC : the CRC value calculated from the data inside the update file
- Flashed : 1 if the flash has been successfully erased and reprogrammed

