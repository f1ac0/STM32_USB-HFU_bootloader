/*
 * command.h
 *
 *  Created on: 20 juin 2023
 *      Author: f1ac0
 */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef _COMMAND_H
#define _COMMAND_H

#ifdef __cplusplus
 extern "C" {
#endif
   
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* User configurations -------------------------------------------------------*/
//Number of main loop cycles to wait for the USB storage device to be detected before jumping to the user App
//A value too low may not work for devices that are slow to power up
//A high value may have an impact on the user App function (eg a HID device not ready prevent to enter a boot menu)
#define WAIT_CYCLES 1048576

//Filenames
#define DUMP_FILENAME  "0:Dual-USB-Controller-Adapter_dump.bin"
#define FLASH_FILENAME "0:Dual-USB-Controller-Adapter.upd"
#define LOG_FILENAME   "0:Dual-USB-Controller-Adapter_log.txt"

//CRC check
//Uncomment the following line if the flash file has its CRC32 appended at the end of the file, after the data
//This CRC32 will then not be flashed but used to check integrity before flashing
//It is suggested to use a file extension different than .bin, eg .upd
#define CRC32_AT_END_OF_FILE

//Dump content before flashing
//Uncomment the following line to write the previous application content before update when !_FS_READONLY
#define DUMP_BEFORE_FLASH

//Write log
//Uncomment the following line to write a log file of the update operation when !_FS_READONLY
#define WRITE_LOG

//This value can be equal to (512 * x) according to RAM size availability with x=[1, 128]
//In this project x is fixed to 64 => 512 * 64 = 32768bytes = 32 Kbytes
#define BUFFER_SIZE    ((uint16_t)512*64)

//The address from where user application will be loaded.
//It must be on a flash sector boundary after the size of our actual bootloader
//If our bootloader takes ~ 22KB then the app can be at 32KB which is sector 2
#define APPLICATION_ADDRESS        (uint32_t)0x08008000

//The flash sectors to check for write protection
//STM32F205 has up to 12 sectors that are 16k 16k 16k 16k 64k and then 128k for the remaining ones
//Bit set to 1 means write protection not active on this sector
//We can check all sectors with OB_WRP_SECTOR_All or only the ones occupied by the application
#define FLASH_SECTOR_TO_CHECK_WRP (uint32_t)0x00000FFC

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
#define BOOTLOADER_FLASH_SIZE (APPLICATION_ADDRESS - FLASH_BASE)

/* Exported macros -----------------------------------------------------------*/
//Check that vector table of application is within range
#define DETECT_APP() (((*(__IO uint32_t*)APPLICATION_ADDRESS) & 0x2FFE0000 ) == 0x20000000)

//Get data bytes in file depending of the compilation options
#ifdef CRC32_AT_END_OF_FILE
#define DATA_SIZE() (f_size(&file) - 4)
#else
#define DATA_SIZE() (f_size(&file))
#endif

/* Exported functions ------------------------------------------------------- */ 
uint32_t COMMAND_CHECK(void);
void COMMAND_DUMP(void);
void COMMAND_FLASH(void);
void COMMAND_UPDATE(void);
void COMMAND_JUMP(void);

extern uint8_t app_detected;
extern uint8_t app_flashed;
extern uint8_t diskprocess;
extern uint32_t cyclesCount;


#ifdef __cplusplus
}
#endif

#endif  /* _COMMAND_H */

