/*
 * Copyright (c) 2012 by Paweł Lebioda <pawel.lebioda89@gmail.com>
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _SD_CONFIG_H
#define _SD_CONFIG_H

#include <avr/io.h>

/* IO config */
#define SD_DETECT_PORT		PORTE
#define SD_DETECT_DDR		DDRE
#define SD_DETECT_PIN		PINE
#define SD_DETECT		6

#define SD_WP_PORT		PORTE
#define SD_WP_DDR		DDRE
#define SD_WP_PIN		PINE
#define SD_WP			2	

#define SD_CS_PORT		PORTB
#define SD_CS_DDR		DDRB
#define SD_CS_PIN		PINB
#define SD_CS			4

/* Preffered block size */
#define SD_BLOCK_SIZE		512

/* Checking CRC checksum for readed block */
#define SD_CRC_SUPPORT		0

#endif //_SD_CONFIG_H