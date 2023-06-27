/**
  * The purpose of this code is to calculate the CRC32 of a file with the same algorithm as the STM32 MCUs CRC peripheral
  * when using its default configuration. A typical application is to embed the CRC32 inside firmware updates, giving a
  * way for the bootloader on the device to confirm that it has not been altered before actually doing the update.
  * I spent hours trying to get the same CRC32 value from my STM32 MCU and other tools installed on my computer.
  * CRC32 is no standard and every tool does it in a way different from the other, giving different CRC32 values.
  * This code is using a source provided by ST inside AN4187, originally for speed comparison purposes on the device itself:
  * https://www.st.com/en/embedded-software/stsw-stm32an4187.html
  *
  * f1ac0, 27/06/2023, https://github.com/f1ca0/STM32_USB-HFU_bootloader
  * gcc stm-crc32.c -o stm-crc32
  */


#include <stdio.h>
#include <stdint.h> //for int types
#include <sys/stat.h> //for file size

#define POLYSIZE CRC_PolSize_32
#define POLYNOME 0x04C11DB7
#define POLYRETURN uint32_t
#define DATATYPE uint32_t
#define MSB_MASK 0x80000000
#define INITIAL_CRC_VALUE 0xFFFFFFFF
#define CRCUPPER sizeof(DATATYPE)*8 /* CRC software upper limit */
#define CRC_SHIFT 1

#define BUFFER_SIZE    512 /* data buffer size */

uint32_t Software_ComputedCRC = INITIAL_CRC_VALUE;
static DATATYPE DataBuffer[BUFFER_SIZE];

/**
  * @brief  Algorithm implementation of the CRC
  * @param  Crc: specifies the previous Crc data
  * @param  Data: specifies the input data
  * @retval Crc: the CRC result of the input data
  */

POLYRETURN CrcSoftwareFunc(POLYRETURN Initial_Crc, DATATYPE Input_Data, POLYRETURN POLY)
{
  uint8_t bindex = 0;
  POLYRETURN Crc = 0;

  /* Initial XOR operation with the previous Crc value */
  Crc = Initial_Crc ^ Input_Data;

  /* The CRC algorithm routine */
  for (bindex = 0; bindex < CRCUPPER; bindex = bindex + 1)
  {
    if (Crc & MSB_MASK)
    {
      Crc = (Crc << CRC_SHIFT) ^ POLY;
    }
    else
    {
      Crc = (Crc << CRC_SHIFT);
    }
  }
  return Crc;
}




int main(int argc, char *argv[])
{
    FILE *srcf;
    FILE *dstf;
	uint16_t hindex = 0;

	uint8_t i;
	uint8_t error=0;
	uint8_t eof=0;
	char *srcn=NULL;
	char *dstn=NULL;
	uint32_t readElem;

//Parse arguments
	for (i = 1; i < argc; i++){
		if (argv[i][0] == '-') {
			if (argv[i][1] == 'o') {
				if(!dstn && (i+1 < argc)) {
					dstn=argv[i+1];
					i++;
				} else error++;
			
			} else error++;
		}else{
			if(!srcn) {
				srcn=argv[i];
			} else error++;
		}
	}

	if(!srcn) error++;

	if(error){
		printf("stm-crc32 [-o dest_file] source_file\nCompute the CRC32 of the source file with the same algorithm as the STM32 MCUs CRC peripheral when using its default configuration.\nWhen a destination filename is provided, copy the file content and append the CRC at the end.");
		return 1;
	}


//Check input file size is multiple of 4 bytes
	struct stat st;
    	if(stat(srcn, &st) != 0) {
    		printf ("Source stat error\n");
        	return 1;
    	}
    	readElem= st.st_size;
	if(readElem & 3) {
		printf ("Source size is not multiple of 4 bytes\n");
		return 1;
	}

//Open files
	srcf= fopen(srcn, "rb");
	if(!srcf) {
		printf ("Source open error\n");
		return 1;
	}
	
	if(dstn){
		dstf= fopen(dstn, "wb");
		if(!dstf) {
			printf ("Destination open error\n");
			return 1;
		}
	}

//Iterate buffer processing through the entire source file
	while(!eof && !error)
	{
  		readElem=fread(DataBuffer, sizeof(DATATYPE), BUFFER_SIZE, srcf);
  		
  		if(readElem != BUFFER_SIZE)
  			eof++;
  
//Compute the DataBuffer table CRC
  		for (hindex = 0; hindex < readElem; hindex = hindex + 1)
  		{
    		Software_ComputedCRC = CrcSoftwareFunc(Software_ComputedCRC, ((uint32_t*)DataBuffer)[hindex], POLYNOME);
  		}
  
//Copy the data to the destination file
		if(dstf) {
  			fwrite(DataBuffer, sizeof(DATATYPE), readElem, dstf);
  			if(ferror (dstf)) {
				printf ("Write error\n");
				error++;
			}
		}
	}


//Write CRC at the end of output file or display it
	if(dstf) {
		fwrite(&Software_ComputedCRC, 4, 1, dstf);
		if(ferror (dstf)) {
      		printf ("Write error\n");
      		error++;
    	}
		fclose(dstf);
	} else {
		printf("%08x\n",Software_ComputedCRC);
	}
	fclose(srcf);

    return 0;
}
