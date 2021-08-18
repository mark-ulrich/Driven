#ifndef __TYPES_H__
#define __TYPES_H__

#pragma once

#include <stdbool.h>
#include <stdint.h>


typedef int8_t  i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef u8  bit_t;
typedef u8  byte_t;
typedef u16 word_t; 

typedef byte_t reg8_t;
typedef word_t reg16_t; 


#define internal     static


#endif    /* __TYPES_H__ */
