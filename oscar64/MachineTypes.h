#pragma once

typedef	unsigned char	 uint8;
typedef unsigned short	uint16;
typedef unsigned int	uint32;
typedef	signed char	 int8;
typedef signed short	int16;
typedef signed short	int32;

static const uint8 BC_REG_IP = 0x19;
static const uint8 BC_REG_ACCU = 0x1b;
static const uint8 BC_REG_ADDR = 0x1f;
static const uint8 BC_REG_STACK = 0x23;
static const uint8 BC_REG_LOCALS = 0x25;

static const uint8 BC_REG_TMP = 0x43;
static const uint8 BC_REG_TMP_SAVED = 0x53;

