/*  Copyright 2014 STMicroelectonics, Inc.  All Rights Reserved.
 *
 *  Permission is granted to use this tool to generate HTTPD filesystem
 *  images for the SPWF01Sx Wifi Module Family.  All other
 *  uses are prohibited.
 *
 *  Compile with 'gcc -Wall -o httpd_gen httpd_gen.c'
 *
 *  Usage:  Create a directory for your filesystem
 *           cd $httpd_fs
 *           httpd_gen outfile.img *
 *           cd -
 *
 *  You may then load this image into the module with the AT+S.HTTPDFSUPDATE
 *  command.
*/
//  Notes:
//  - to correctly manage one level subfolder use: httpd_gen outfile.img *.* */*
//  - the length of the (path + filename) must be < 32 chars


#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define u32 uint32_t
#define u8 uint8_t

#define FLASH_HTTPD_COOKIE 0x00228801

struct spi_flash_httpd_file {
	u32 offset;  /* From FLASH_HTTPD_HEADER_OFFSET */
	u32 len;
	u8 name[32];
	u8 flags;
	u8 pad[3];
};

struct spi_flash_httpd_fs {
	u32 id; /* Set to FLASH_HTTPD_COOKIE */
	u32 len; /* Overall length of fs blob */
	u32 num;
	struct spi_flash_httpd_file entries[];
//      data follows..	
//	u32 crc;  /* CRC32 of image follows image, LE */
};

#define BUF_LEN 2048

/* CRC32 code that mirrors the STM32's engine. */
static u32 update_crc(u32 Crc, u8 *Buffer, u32 Size)
{
  Size = Size >> 2; // /4
  
  while (Size--) {
    static const u32 CrcTable[16] = { // Nibble lookup table for 0x04C11DB7 polynomial
      0x00000000,0x04C11DB7,0x09823B6E,0x0D4326D9,0x130476DC,0x17C56B6B,0x1A864DB2,0x1E475005,
      0x2608EDB8,0x22C9F00F,0x2F8AD6D6,0x2B4BCB61,0x350C9B64,0x31CD86D3,0x3C8EA00A,0x384FBDBD };
    
    Crc = Crc ^ *((u32 *)Buffer); // Apply all 32-bits
    
    Buffer += 4;
    
    // Process 32-bits, 4 at a time, or 8 rounds
    
    Crc = (Crc << 4) ^ CrcTable[Crc >> 28]; // Assumes 32-bit reg, masking index to 4-bits
    Crc = (Crc << 4) ^ CrcTable[Crc >> 28]; //  0x04C11DB7 Polynomial used in STM32
    Crc = (Crc << 4) ^ CrcTable[Crc >> 28];
    Crc = (Crc << 4) ^ CrcTable[Crc >> 28];
    Crc = (Crc << 4) ^ CrcTable[Crc >> 28];
    Crc = (Crc << 4) ^ CrcTable[Crc >> 28];
    Crc = (Crc << 4) ^ CrcTable[Crc >> 28];
    Crc = (Crc << 4) ^ CrcTable[Crc >> 28];
  }
  
  return(Crc);
}

int main(int argc, char **argv) {
	struct spi_flash_httpd_fs hs;
	struct spi_flash_httpd_file **hf;
	int i;
	u32 offset = 0;
	u32 crc = 0xffffffff;
	FILE *f_out;
	u8 buf[BUF_LEN];

	if (argc < 3) {
		fprintf(stderr, "\nusage: argv[0] outfile fname1 [ fname2 [ ... fnameN ] ]\n\n");
		exit(1);
	}

	hs.id = FLASH_HTTPD_COOKIE;
	hs.num = 0;
	hs.len = sizeof(hs);

	hf = malloc(sizeof(*hf) * argc);
	memset(hf, 0, sizeof(*hf) * argc);

	/* Walk files, build headers */
	for (i = 2 ; i < argc ; i++) {
		struct stat sbuf;
		struct spi_flash_httpd_file *hff;

		stat(argv[i], &sbuf);

		hff = malloc(sizeof(*hff));
		memset(hff, 0, sizeof(*hff));
		hf[i-2] = hff;

		hff->offset = offset;
		hff->len = sbuf.st_size;
		offset+= hff->len;
		hff->name[0] = '/';
		strncpy(hff->name + 1, argv[i], sizeof(hff->name)-2);
		hff->name[sizeof(hff->name)-1] = 0;
		hff->flags = 0x2; /* FSDATA_EXTERNAL */
		hs.num++;
	}
	hs.len += sizeof(struct spi_flash_httpd_file) * hs.num;

	/* Correct offsets */
	for (i = 0 ; i < hs.num ; i++) {
		hs.len += hf[i]->len;
		hf[i]->offset += sizeof(hs) + sizeof(struct spi_flash_httpd_file) * hs.num;
	}

	/* Serialize out file */
	f_out = fopen(argv[1], "w+");
	fwrite(&hs, 1, sizeof(hs), f_out);
	for (i = 0 ; i < hs.num ; i++ ) {
		fwrite(hf[i], 1, sizeof(*hf[i]), f_out);
	}
	for (i = 0 ; i < hs.num ; i++) {
		FILE *f_in;
		int c;

		f_in = fopen(argv[2 + i], "r");

		while((c = fread(buf, 1, BUF_LEN, f_in)) > 0) {
			fwrite(buf, 1, c, f_out);
		}	

		fclose(f_in);
	}
	/* Pad to multiple of 4 bytes */
	if (hs.len % 4) {
		memset(buf, 0, BUF_LEN);
		fwrite(buf, 1, 4 - hs.len % 4, f_out);
	}
	/* Reset to compute CRC */
	fseek(f_out, 0, SEEK_SET);

	while((i = fread(buf, 4, BUF_LEN/4, f_out)) > 0) {
		crc = update_crc(crc, buf, i * 4);
	}
	fwrite(&crc, 4, 1, f_out);
	fclose(f_out);
	       
	return 0;
}
