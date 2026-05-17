#ifndef __DS1302_H
#define __DS1302_H

#include "reg52.h"
#include "intrins.h"

typedef struct
{
    unsigned char year;
    unsigned char month;
    unsigned char date;
    unsigned char hour;
    unsigned char minute;
    unsigned char second;
    unsigned char week;
} RTC_TIME;

sbit SCK = P1^7;
sbit SDA = P2^3;
sbit RST = P1^3;

void DS1302_InitDefault(void);
void DS1302_Read(RTC_TIME *t);
void DS1302_Write(const RTC_TIME *t);

#endif
