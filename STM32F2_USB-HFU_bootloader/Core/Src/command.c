/*
 * command.c
 *
 *  Created on: 20 juin 2023
 *      Author: f1ac0
 */

/* Includes ------------------------------------------------------------------*/
#include "command.h"
#include "fatfs.h"

/* Private typedef -----------------------------------------------------------*/
/* Private defines -----------------------------------------------------------*/

/* Private macros ------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
uint8_t app_detected;
uint8_t app_flashed;
uint8_t diskprocess;
uint32_t cyclesCount;
uint32_t crc;

uint8_t RAM_Buf[BUFFER_SIZE];
extern FATFS USBHFatFS;
extern USBH_HandleTypeDef hUsbHostFS;
extern CRC_HandleTypeDef hcrc;

/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

int8_t getFlashSector(uint32_t address)
{
	if(address<FLASH_BASE) return -1;
	if(address<FLASH_BASE+16384) return 0;
	if(address<FLASH_BASE+2*16384) return 1;
	if(address<FLASH_BASE+3*16384) return 2;
	if(address<FLASH_BASE+4*16384) return 3;
	if(address<FLASH_BASE+131072) return 4;
	if(address<FLASH_BASE+2*131072) return 5;
	if(address<FLASH_BASE+3*131072) return 6;
	if(address<FLASH_BASE+4*131072) return 7;
	if(address<FLASH_BASE+5*131072) return 8;
	if(address<FLASH_BASE+6*131072) return 9;
	if(address<FLASH_BASE+7*131072) return 10;
	if(address<FLASH_BASE+8*131072) return 11;
	return -1;
}

/**
  * @brief  Compare flash memory to the content of a file
  * @param  None
  * @retval
		//0= file check successful and Flash memory is the same as file content
		//1= file open error
		//2= file size not multiple of 32 bits
		//4= file read error
		//8= vector table in file mismatch
		//16= read bytes and file size mismatch
		//32= CRC32 from file read error
		//64= CRC32 in file mismatch
		//>= address of first difference
  */
uint32_t COMMAND_CHECK(void)
{
	FIL file;
	uint32_t result=1;

	//Open the file if exists
	if (f_open(&file, FLASH_FILENAME, FA_READ) == FR_OK)
	{
		uint8_t error=0;
		uint8_t eof=0;
		uint32_t flashAddr=APPLICATION_ADDRESS;
		uint32_t diffAddr=0;
		uint32_t bytesTotal=DATA_SIZE();
		uint32_t bytestoRead;
		uint32_t bytesDone=0;
		uint16_t bytesRead;
		uint16_t bufOffset;

	//Check that the file size is multiple of 32 bits ; I believe all STM32 firmwares are, and CRC calculation work with 32 bits data
		if((file.fsize & 0x3) !=0){
			error|=2;
			eof++;
		} else {

	//Iterate through the entire file
			while (!eof)
			{

	//Calculate amount of data to read this iteration
				bytestoRead=bytesTotal-bytesDone;
				if(bytestoRead>BUFFER_SIZE)
					bytestoRead=BUFFER_SIZE;

	//Read data and check end of file
				if(f_read(&file, RAM_Buf, bytestoRead, (void *)&bytesRead) != FR_OK)
					error|=4;
				if (bytesRead < BUFFER_SIZE)
					eof++;

	//Check Vector Table of application is in expected range
				if((bytesDone==0) && (bytesRead>4)) //beginning of file
					if(((*(__IO uint32_t*)RAM_Buf) & 0x2FFE0000 ) != 0x20000000)
						error|=8;

	//CRC
				crc=HAL_CRC_Accumulate(&hcrc, (uint32_t *)RAM_Buf, bytesRead/4);

	//Compare the data we read to what we already have in flash
				bufOffset=0;
				while (!diffAddr && (bufOffset<bytesRead))
				{
					if (*((__IO uint16_t*)(flashAddr)) == *((uint16_t*)(RAM_Buf+bufOffset)))
					{
						flashAddr = flashAddr + 2;
						bufOffset = bufOffset + 2;
					}else{
						diffAddr=flashAddr;
					}
				}
	//Check the amount done so far
				bytesDone+=bytesRead;
			}
			if(bytesDone!=bytesTotal)
				error|=16;

	//Read CRC value from file and compare it to the one calculated
#if defined(CRC32_AT_END_OF_FILE)
			if((f_read(&file, RAM_Buf, 4, (void *)&bytesRead) != FR_OK) || (bytesRead!=4))
				error|=32;
			if(crc != *(uint32_t *)RAM_Buf)
				error|=64;
#endif
		}

		result=error ? error : diffAddr;

	//Close the file
		f_close (&file);
	}//end of Open the file

	return result;
}

/**
  * @brief Write the content of flash memory to a file
  * @param  None
  * @retval None
  */
#if(defined(DUMP_BEFORE_FLASH) && (!_FS_READONLY))
void COMMAND_DUMP(void)
{
	FIL file;

	//Check flash is not read protected
	FLASH_OBProgramInitTypeDef OptionsBytesStruct;
	HAL_FLASHEx_OBGetConfig(&OptionsBytesStruct);
	if (OptionsBytesStruct.RDPLevel == OB_RDP_LEVEL_0)
	{

	//Remove destination file if it exists
		f_unlink(DUMP_FILENAME);

	//Open the destination file
		if(f_open(&file, DUMP_FILENAME, FA_CREATE_ALWAYS | FA_WRITE) == FR_OK)
		{

	//Write flash content to file
			uint32_t flashAddr=APPLICATION_ADDRESS; //otherwise FLASH_BASE to also dump the bootloader
			uint32_t flashEndAddr = FLASH_BASE + ((*((uint16_t*)FLASHSIZE_BASE)) << 10);
			uint8_t error=0;
			uint8_t eof=0;
			uint16_t bytesWritten;
			uint16_t bufOffset;

			while(!error && !eof)
			{
				bufOffset=0;
				while(!eof && bufOffset<BUFFER_SIZE)
				{
					RAM_Buf[bufOffset++] = (*(uint8_t*)(flashAddr++)); //Is buffer necessary ? Can't we f_write from flash ?
					if(flashAddr>=flashEndAddr)
						eof++;
				}
				if(f_write(&file, RAM_Buf, bufOffset, (void *)&bytesWritten) != FR_OK)
					error++;
				if(bytesWritten != bufOffset)
					error++;
			}

	//Close the file
			f_close (&file);

		}//end of Open the destination file
	}//end of Check flash is not read protected
}
#endif

/**
  * @brief  Write flash memory from a file
  * @param  None
  * @retval None
  */
void COMMAND_FLASH(void)
{
	FIL file;

	//Open the file if exists
	if (f_open(&file, FLASH_FILENAME, FA_READ) == FR_OK)
	{
	//Check file size<available flash
		uint32_t bytesTotal=DATA_SIZE();
		const size_t availableFlashSize = ((*((uint16_t*)FLASHSIZE_BASE)) << 10) - BOOTLOADER_FLASH_SIZE;
		if ((file.fsize>4) && (bytesTotal <= availableFlashSize))
		{

	//Check flash is not write protected
			FLASH_OBProgramInitTypeDef OptionsBytesStruct;
			HAL_FLASHEx_OBGetConfig(&OptionsBytesStruct);
			if (((~OptionsBytesStruct.WRPSector) & FLASH_SECTOR_TO_CHECK_WRP) == 0x00) //WRPSector bits are 1 when not protected
			{
	//Erase sectors required by update
				int8_t sector=getFlashSector(APPLICATION_ADDRESS);
				int8_t lastSector=getFlashSector(APPLICATION_ADDRESS+bytesTotal);
				FLASH_EraseInitTypeDef EraseInitStruct;
				EraseInitStruct.TypeErase   = FLASH_TYPEERASE_SECTORS;
				EraseInitStruct.Banks       = FLASH_BANK_1;
				EraseInitStruct.Sector      = sector;
				EraseInitStruct.NbSectors   = lastSector-sector+1; //erase up to the file size instead of all sectors
				EraseInitStruct.VoltageRange= FLASH_VOLTAGE_RANGE_3;
				uint32_t SectorError = 0;
				HAL_FLASH_Unlock();
				if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) == HAL_OK)
				{

	//Program the update
					uint32_t flashAddr=APPLICATION_ADDRESS;
					uint8_t error=0;
					uint8_t eof=0;
					uint32_t bytesDone=0;
					uint16_t bytesRead;
					uint16_t bufOffset;
					uint16_t bytestoRead;

					//Iterate through the entire file
					while (!error && !eof)
					{
						//Calculate amount of data to read this iteration
						bytestoRead=bytesTotal-bytesDone;
						if(bytestoRead>BUFFER_SIZE)
							bytestoRead=BUFFER_SIZE;

						//Read data and check end of file
						if(f_read(&file, RAM_Buf, bytestoRead, (void *)&bytesRead) != FR_OK)
							error++;
						if (bytesRead < BUFFER_SIZE)
							eof++;

						//Program the data we read
						bufOffset=0;
						while (!error && (bufOffset<bytesRead))
						{
							if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, flashAddr, *((uint32_t*)(RAM_Buf+bufOffset))) == HAL_OK)
							{
								flashAddr = flashAddr + 4;
								bufOffset = bufOffset + 4;
							}else{
								error++;
							}
						}

						//Check the amount done so far (could be omitted)
						bytesDone+=bytesRead;
					}
					if(bytesDone != bytesTotal)
						error++;

	//set app_flashed
					if(error==0)
						app_flashed=1;

				}//end of Erase pages required by update
				HAL_FLASH_Lock();

			}//end of Check flash is not write protected
		}//end of Check file size<available flash
	//Close the file
		f_close (&file);
	}//end of Open the file
}

/**
  * @brief  Process the update of the user App
  * @param  None
  * @retval None
  */
void COMMAND_UPDATE(void)
{
	//Mount the filesystem
	if(f_mount(&USBHFatFS,"",0) == FR_OK)
	{

	//Check that there is no trouble with the file and its content is different than the one we already have in flash
		uint32_t check=COMMAND_CHECK();
	    if(check>=APPLICATION_ADDRESS)
	    {

	//If file is OK and content differs, dump the actual flash content and flash the new file
#if(defined(DUMP_BEFORE_FLASH) && (!_FS_READONLY))
	    	COMMAND_DUMP();
#endif
	    	COMMAND_FLASH();
	    }

	//DEBUG log file
#if(defined(WRITE_LOG) && (!_FS_READONLY))
		FIL file;
		if(f_open(&file, LOG_FILENAME, FA_OPEN_ALWAYS | FA_WRITE) == FR_OK)
		{
			f_lseek(&file, file.fsize);
			uint16_t bytesWritten;
			sprintf((char*)RAM_Buf,"Cycles=%08lx Check=%08lx CRC=%08lx Flashed=%u\n",cyclesCount,check,crc,app_flashed);
			f_write(&file, RAM_Buf, strlen((char*)RAM_Buf), (void *)&bytesWritten);
			f_close(&file);
			HAL_Delay(1000U);
		}
#endif

	//Unmount the filesystem
		f_mount(NULL,"",1);

	}//end of Mount the filesystem
}


/**
  * @brief  Jump to user App
  * @param  None
  * @retval None
  */
void COMMAND_JUMP(void)
{
	typedef  void (*pFunction)(void);
	pFunction Jump_To_Application;
	uint32_t JumpAddress;

    //Check Vector Table of application is in expected range
    if(DETECT_APP())
    {
    	//Deactivate peripherals otherwise the interrupts they generate will prevent the App from booting correctly
    	USBH_Stop(&hUsbHostFS);
        USBH_DeInit(&hUsbHostFS);
        USBH_LL_DeInit(&hUsbHostFS);

		//Jump to application
		JumpAddress = *(__IO uint32_t*) (APPLICATION_ADDRESS + 4);
		Jump_To_Application = (pFunction) JumpAddress;
		__set_MSP(*(__IO uint32_t*) APPLICATION_ADDRESS);
		Jump_To_Application();
    }
}
