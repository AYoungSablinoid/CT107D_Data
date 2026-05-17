#include "ds1302.h"

code unsigned char ds1302_addr_w[7] = {0x80, 0x82, 0x84, 0x86, 0x88, 0x8a, 0x8c};
code unsigned char ds1302_addr_r[7] = {0x81, 0x83, 0x85, 0x87, 0x89, 0x8b, 0x8d};
#define DS1302_DEFAULT_YEAR 26
#define DS1302_DEFAULT_WEEK 4

static unsigned char bcd_to_dec(unsigned char bcd)
{
    return (bcd >> 4) * 10 + (bcd & 0x0f);
}

static unsigned char dec_to_bcd(unsigned char dec)
{
    return (dec / 10) * 16 + (dec % 10);
}

static void ds1302_write_byte(unsigned char dat)
{
    unsigned char i;
    for(i = 0; i < 8; i++)
    {
        SCK = 0;
        SDA = dat & 0x01;
        dat >>= 1;
        SCK = 1;
    }
}

static void ds1302_write(unsigned char address, unsigned char dat)
{
    RST = 0;
    _nop_();
    SCK = 0;
    _nop_();
    RST = 1;
    _nop_();
    ds1302_write_byte(address);
    ds1302_write_byte(dat);
    RST = 0;
}

static unsigned char ds1302_read(unsigned char address)
{
    unsigned char i, dat = 0;

    RST = 0;
    _nop_();
    SCK = 0;
    _nop_();
    RST = 1;
    _nop_();

    ds1302_write_byte(address);

    for(i = 0; i < 8; i++)
    {
        SCK = 0;
        dat >>= 1;
        if(SDA)
            dat |= 0x80;
        SCK = 1;
    }

    RST = 0;
    _nop_();
    SCK = 0;
    _nop_();
    SCK = 1;
    _nop_();
    SDA = 0;
    _nop_();
    SDA = 1;
    _nop_();

    return dat;
}

void DS1302_Read(RTC_TIME *t)
{
    t->second = bcd_to_dec(ds1302_read(ds1302_addr_r[0]) & 0x7f);
    t->minute = bcd_to_dec(ds1302_read(ds1302_addr_r[1]) & 0x7f);
    t->hour = bcd_to_dec(ds1302_read(ds1302_addr_r[2]) & 0x3f);
    t->date = bcd_to_dec(ds1302_read(ds1302_addr_r[3]) & 0x3f);
    t->month = bcd_to_dec(ds1302_read(ds1302_addr_r[4]) & 0x1f);
    t->week = bcd_to_dec(ds1302_read(ds1302_addr_r[5]) & 0x07);
    t->year = bcd_to_dec(ds1302_read(ds1302_addr_r[6]));
}

void DS1302_Write(const RTC_TIME *t)
{
    ds1302_write(0x8e, 0x00);
    ds1302_write(ds1302_addr_w[0], dec_to_bcd(t->second));
    ds1302_write(ds1302_addr_w[1], dec_to_bcd(t->minute));
    ds1302_write(ds1302_addr_w[2], dec_to_bcd(t->hour));
    ds1302_write(ds1302_addr_w[3], dec_to_bcd(t->date));
    ds1302_write(ds1302_addr_w[4], dec_to_bcd(t->month));
    ds1302_write(ds1302_addr_w[5], dec_to_bcd(t->week));
    ds1302_write(ds1302_addr_w[6], dec_to_bcd(t->year));
    ds1302_write(0x8e, 0x80);
}

void DS1302_InitDefault(void)
{
    RTC_TIME t;

    DS1302_Read(&t);
    if(t.month == 0 || t.month > 12 || t.date == 0 || t.date > 31 || t.week == 0 || t.week > 7)
    {
        t.year = DS1302_DEFAULT_YEAR;
        t.month = 1;
        t.date = 1;
        t.hour = 0;
        t.minute = 0;
        t.second = 0;
        t.week = DS1302_DEFAULT_WEEK;
        DS1302_Write(&t);
    }
}
