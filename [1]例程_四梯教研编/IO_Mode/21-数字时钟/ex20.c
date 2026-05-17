#include "reg52.h"
#include "intrins.h"
#include "ds1302.h"
#include "i2c.h"

sfr AUXR = 0x8E;
sfr P4 = 0xC0;

sbit rs = P2^0;
sbit rw = P2^1;
sbit en = P1^2;

sbit R1 = P3^0;
sbit R2 = P3^1;
sbit R3 = P3^2;
sbit R4 = P3^3;
sbit C1 = P4^4;
sbit C2 = P4^2;
sbit C3 = P3^5;
sbit C4 = P3^4;

#define KEY_NONE  255
#define KEY_COLON 10
#define KEY_BACK  11
#define KEY_ENTER 12
#define KEY_MODE  13
#define KEY_ALARM 14
#define KEY_CHIME 15

#define MODE_CLOCK      0
#define MODE_SET_TIME   1
#define MODE_ALARM_VIEW 2
#define MODE_ALARM_EDIT 3

#define ALARM_COUNT 2

code unsigned char seg_tab[] = {0xc0, 0xf9, 0xa4, 0xb0, 0x99, 0x92, 0x82, 0xf8, 0x80, 0x90, 0xFF, 0xbf};
code unsigned char edit_field_len[] = {2, 2, 2, 2, 2, 2, 1};
code unsigned char edit_field_min[] = {0, 1, 1, 0, 0, 0, 1};
code unsigned char edit_field_max[] = {99, 12, 31, 23, 59, 59, 7};
code unsigned char edit_field_code[] = {'Y', 'M', 'D', 'h', 'm', 's', 'w'};

typedef struct
{
    unsigned char hour;
    unsigned char minute;
    unsigned char enable;
} ALARM_INFO;

unsigned char dspbuf[8] = {10, 10, 10, 10, 10, 10, 10, 10};
unsigned char dspcom = 0;

unsigned int ms_count = 0;
bit flag_10ms = 0;
bit flag_1s = 0;

RTC_TIME g_rtc;
ALARM_INFO alarms[ALARM_COUNT];

unsigned char mode = MODE_CLOCK;
unsigned char screen_index = 0;
unsigned char screen_hold_seconds = 0;
bit screen_pause = 0;

bit chime_enable = 1;
bit alarm_master_enable = 1;
unsigned char alarm_view_index = 0;

unsigned int buzz_ms = 0;
unsigned char last_hourly_hour = 0xff;

unsigned char edit_values[7];
unsigned char edit_field = 0;
unsigned char edit_digits = 0;
unsigned int edit_value = 0;

unsigned char alarm_edit_digits = 0;
unsigned int alarm_edit_value = 0;

void latch_select(unsigned char channel);
void led_all_off(void);
void buzzer_output(bit on);
void trigger_beep(unsigned int t);
void hardware_init(void);
void timer0_init(void);
void display_scan(void);
void update_seg_clock_screen(void);
void update_seg_date_screen(void);
void update_seg_week_screen(void);
void update_seg_buffer(void);
void delay_ms(unsigned char ms);
void lcd_write_cmd(unsigned char cmd);
void lcd_write_data(unsigned char dat);
void lcd_init(void);
void lcd_set_pos(unsigned char x, unsigned char y);
void lcd_print(unsigned char x, unsigned char y, unsigned char *s);
void show_clock_lcd(void);
void show_time_edit_lcd(void);
void show_alarm_lcd(void);
void load_settings_from_eeprom(void);
void save_settings_to_eeprom(void);
unsigned char key_scan_raw(void);
unsigned char key_scan(void);
unsigned char is_field_valid(unsigned char idx, unsigned int val);
void enter_time_edit_mode(void);
void enter_alarm_view_mode(void);
void enter_alarm_edit_mode(void);
void exit_to_clock_mode(void);
void handle_key(unsigned char key);
void handle_clock_key(unsigned char key);
void handle_time_edit_key(unsigned char key);
void handle_alarm_view_key(unsigned char key);
void handle_alarm_edit_key(unsigned char key);
void check_chime_and_alarm(void);

void latch_select(unsigned char channel)
{
    switch(channel)
    {
    case 0:
        P2 = (P2 & 0x1f) | 0x00;
        break;
    case 4:
        P2 = (P2 & 0x1f) | 0x80;
        break;
    case 5:
        P2 = (P2 & 0x1f) | 0xa0;
        break;
    case 6:
        P2 = (P2 & 0x1f) | 0xc0;
        break;
    case 7:
        P2 = (P2 & 0x1f) | 0xe0;
        break;
    }
}

void led_all_off(void)
{
    latch_select(4);
    P0 = 0xff;
    latch_select(0);
}

void buzzer_output(bit on)
{
    latch_select(5);
    P0 = 0x00;
    if(on)
    {
        P0 |= 0x40;
    }
    latch_select(0);
}

void trigger_beep(unsigned int t)
{
    if(t > buzz_ms)
    {
        buzz_ms = t;
    }
}

void hardware_init(void)
{
    en = 0;
    P0 = 0xff;
    led_all_off();
    buzzer_output(0);
}

void delay_ms(unsigned char ms)
{
    unsigned char i, j;
    while(ms--)
    {
        i = 11;
        j = 190;
        do
        {
            while(--j);
        }
        while(--i);
    }
}

void lcd_write_cmd(unsigned char cmd)
{
    rw = 0;
    rs = 0;
    latch_select(0);
    P0 = cmd;
    delay_ms(1);
    en = 1;
    delay_ms(1);
    en = 0;
}

void lcd_write_data(unsigned char dat)
{
    rw = 0;
    rs = 1;
    latch_select(0);
    P0 = dat;
    delay_ms(1);
    en = 1;
    delay_ms(1);
    en = 0;
}

void lcd_init(void)
{
    rw = 0;
    en = 0;
    lcd_write_cmd(0x38);
    lcd_write_cmd(0x0c);
    lcd_write_cmd(0x06);
    lcd_write_cmd(0x01);
    delay_ms(5);
}

void lcd_set_pos(unsigned char x, unsigned char y)
{
    if(y == 0)
    {
        lcd_write_cmd(0x80 + x);
    }
    else
    {
        lcd_write_cmd(0xc0 + x);
    }
}

void lcd_print(unsigned char x, unsigned char y, unsigned char *s)
{
    lcd_set_pos(x, y);
    while(*s)
    {
        lcd_write_data(*s++);
    }
}

void timer0_init(void)
{
    AUXR |= 0x80;
    TMOD &= 0xf0;
    TL0 = 0xCD;
    TH0 = 0xD4;
    TF0 = 0;
    TR0 = 1;
    ET0 = 1;
    EA = 1;
}

void display_scan(void)
{
    latch_select(7);
    P0 = 0xff;
    latch_select(0);

    latch_select(6);
    P0 = (1 << dspcom);
    latch_select(0);

    latch_select(7);
    P0 = seg_tab[dspbuf[dspcom]];
    latch_select(0);

    if(++dspcom == 8)
    {
        dspcom = 0;
    }
}

void update_seg_clock_screen(void)
{
    dspbuf[0] = g_rtc.hour / 10;
    dspbuf[1] = g_rtc.hour % 10;
    dspbuf[2] = 11;
    dspbuf[3] = g_rtc.minute / 10;
    dspbuf[4] = g_rtc.minute % 10;
    dspbuf[5] = 11;
    dspbuf[6] = g_rtc.second / 10;
    dspbuf[7] = g_rtc.second % 10;
}

void update_seg_date_screen(void)
{
    unsigned int year_full;

    year_full = 2000 + g_rtc.year;
    dspbuf[0] = year_full / 1000;
    dspbuf[1] = year_full % 1000 / 100;
    dspbuf[2] = year_full % 100 / 10;
    dspbuf[3] = year_full % 10;
    dspbuf[4] = g_rtc.month / 10;
    dspbuf[5] = g_rtc.month % 10;
    dspbuf[6] = g_rtc.date / 10;
    dspbuf[7] = g_rtc.date % 10;
}

void update_seg_week_screen(void)
{
    dspbuf[0] = 10;
    dspbuf[1] = 10;
    dspbuf[2] = 10;
    dspbuf[3] = 10;
    dspbuf[4] = 10;
    dspbuf[5] = 0;
    dspbuf[6] = 0;
    dspbuf[7] = g_rtc.week;
}

void update_seg_buffer(void)
{
    if(mode != MODE_CLOCK)
    {
        update_seg_clock_screen();
        return;
    }

    if(screen_index == 0)
    {
        update_seg_clock_screen();
    }
    else if(screen_index == 1)
    {
        update_seg_date_screen();
    }
    else
    {
        update_seg_week_screen();
    }
}

void show_clock_lcd(void)
{
    unsigned char line1[17] = "CLK 00:00:00 W0";
    unsigned char line2[17] = "M Set A Alm C On";

    line1[4] = g_rtc.hour / 10 + '0';
    line1[5] = g_rtc.hour % 10 + '0';
    line1[7] = g_rtc.minute / 10 + '0';
    line1[8] = g_rtc.minute % 10 + '0';
    line1[10] = g_rtc.second / 10 + '0';
    line1[11] = g_rtc.second % 10 + '0';
    line1[14] = g_rtc.week + '0';

    if(!chime_enable)
    {
        line2[14] = 'O';
        line2[15] = 'f';
    }

    lcd_print(0, 0, line1);
    lcd_print(0, 1, line2);
}

void show_time_edit_lcd(void)
{
    unsigned char line1[17] = "SET X=0000      ";
    unsigned char line2[17] = "Num E OK : Next ";
    unsigned int value_show;

    line1[4] = edit_field_code[edit_field];
    value_show = edit_value;

    line1[6] = (value_show / 1000) % 10 + '0';
    line1[7] = (value_show / 100) % 10 + '0';
    line1[8] = (value_show / 10) % 10 + '0';
    line1[9] = value_show % 10 + '0';

    lcd_print(0, 0, line1);
    lcd_print(0, 1, line2);
}

void show_alarm_lcd(void)
{
    unsigned char line1[17] = "ALM1 ON 00:00    ";
    unsigned char line2[17] = "M Edit :Next ETg ";
    ALARM_INFO *a;
    unsigned char hh;
    unsigned char mm;

    a = &alarms[alarm_view_index];
    hh = a->hour;
    mm = a->minute;
    if(mode == MODE_ALARM_EDIT)
    {
        hh = alarm_edit_value / 100;
        mm = alarm_edit_value % 100;
        line2[0] = 'I';
        line2[1] = 'n';
        line2[2] = ' ';
        line2[3] = 'E';
        line2[4] = ' ';
        line2[5] = 'S';
        line2[6] = 'a';
        line2[7] = 'v';
        line2[8] = 'e';
        line2[9] = ' ';
    }
    line1[3] = alarm_view_index + 1 + '0';
    if(!a->enable)
    {
        line1[5] = 'O';
        line1[6] = 'F';
    }
    line1[8] = hh / 10 + '0';
    line1[9] = hh % 10 + '0';
    line1[11] = mm / 10 + '0';
    line1[12] = mm % 10 + '0';

    line2[14] = alarm_master_enable ? 'N' : 'F';

    lcd_print(0, 0, line1);
    lcd_print(0, 1, line2);
}

void load_settings_from_eeprom(void)
{
    if(read_eeprom(0x00) != 0x5a)
    {
        chime_enable = 1;
        alarm_master_enable = 1;
        alarms[0].hour = 7;
        alarms[0].minute = 0;
        alarms[0].enable = 0;
        alarms[1].hour = 18;
        alarms[1].minute = 0;
        alarms[1].enable = 0;
        save_settings_to_eeprom();
        return;
    }

    chime_enable = read_eeprom(0x01) ? 1 : 0;
    alarm_master_enable = read_eeprom(0x02) ? 1 : 0;

    alarms[0].hour = read_eeprom(0x03);
    alarms[0].minute = read_eeprom(0x04);
    alarms[0].enable = read_eeprom(0x05) ? 1 : 0;

    alarms[1].hour = read_eeprom(0x06);
    alarms[1].minute = read_eeprom(0x07);
    alarms[1].enable = read_eeprom(0x08) ? 1 : 0;

    if(alarms[0].hour > 23) alarms[0].hour = 0;
    if(alarms[1].hour > 23) alarms[1].hour = 0;
    if(alarms[0].minute > 59) alarms[0].minute = 0;
    if(alarms[1].minute > 59) alarms[1].minute = 0;
}

void save_settings_to_eeprom(void)
{
    write_eeprom(0x00, 0x5a);
    write_eeprom(0x01, chime_enable);
    write_eeprom(0x02, alarm_master_enable);

    write_eeprom(0x03, alarms[0].hour);
    write_eeprom(0x04, alarms[0].minute);
    write_eeprom(0x05, alarms[0].enable);

    write_eeprom(0x06, alarms[1].hour);
    write_eeprom(0x07, alarms[1].minute);
    write_eeprom(0x08, alarms[1].enable);
}

unsigned char key_scan_raw(void)
{
    R1 = R2 = R3 = R4 = 1;

    R1 = 0;
    if(C1 == 0) return 7;
    if(C2 == 0) return 8;
    if(C3 == 0) return 9;
    if(C4 == 0) return KEY_COLON;

    R1 = 1;
    R2 = 0;
    if(C1 == 0) return 4;
    if(C2 == 0) return 5;
    if(C3 == 0) return 6;
    if(C4 == 0) return KEY_BACK;

    R2 = 1;
    R3 = 0;
    if(C1 == 0) return 1;
    if(C2 == 0) return 2;
    if(C3 == 0) return 3;
    if(C4 == 0) return KEY_ENTER;

    R3 = 1;
    R4 = 0;
    if(C1 == 0) return KEY_MODE;
    if(C2 == 0) return 0;
    if(C3 == 0) return KEY_ALARM;
    if(C4 == 0) return KEY_CHIME;

    R4 = 1;
    return KEY_NONE;
}

unsigned char key_scan(void)
{
    unsigned char k;

    k = key_scan_raw();
    if(k == KEY_NONE)
    {
        return KEY_NONE;
    }

    delay_ms(10);
    if(key_scan_raw() != k)
    {
        return KEY_NONE;
    }

    while(key_scan_raw() != KEY_NONE)
    {
        display_scan();
        delay_ms(1);
    }

    return k;
}

unsigned char is_field_valid(unsigned char idx, unsigned int val)
{
    if(val < edit_field_min[idx] || val > edit_field_max[idx])
    {
        return 0;
    }
    return 1;
}

void enter_time_edit_mode(void)
{
    mode = MODE_SET_TIME;
    edit_values[0] = g_rtc.year;
    edit_values[1] = g_rtc.month;
    edit_values[2] = g_rtc.date;
    edit_values[3] = g_rtc.hour;
    edit_values[4] = g_rtc.minute;
    edit_values[5] = g_rtc.second;
    edit_values[6] = g_rtc.week;

    edit_field = 0;
    edit_digits = 0;
    edit_value = edit_values[0];
    show_time_edit_lcd();
}

void enter_alarm_view_mode(void)
{
    mode = MODE_ALARM_VIEW;
    alarm_view_index = 0;
    show_alarm_lcd();
}

void enter_alarm_edit_mode(void)
{
    ALARM_INFO *a;

    mode = MODE_ALARM_EDIT;
    a = &alarms[alarm_view_index];
    alarm_edit_value = a->hour * 100 + a->minute;
    alarm_edit_digits = 0;
    show_alarm_lcd();
}

void exit_to_clock_mode(void)
{
    mode = MODE_CLOCK;
    show_clock_lcd();
}

void handle_clock_key(unsigned char key)
{
    if(key == KEY_MODE)
    {
        enter_time_edit_mode();
    }
    else if(key == KEY_ALARM)
    {
        enter_alarm_view_mode();
    }
    else if(key == KEY_CHIME)
    {
        chime_enable = !chime_enable;
        save_settings_to_eeprom();
        show_clock_lcd();
        trigger_beep(80);
    }
    else if(key == KEY_COLON)
    {
        screen_pause = !screen_pause;
    }
}

void handle_time_edit_key(unsigned char key)
{
    unsigned char need_digits;

    need_digits = edit_field_len[edit_field];

    if(key <= 9)
    {
        if(edit_digits < need_digits)
        {
            if(edit_digits == 0)
            {
                edit_value = 0;
            }
            edit_value = edit_value * 10 + key;
            edit_digits++;
            show_time_edit_lcd();
        }
        return;
    }

    if(key == KEY_BACK)
    {
        edit_value /= 10;
        if(edit_digits > 0)
        {
            edit_digits--;
        }
        show_time_edit_lcd();
        return;
    }

    if(key == KEY_ENTER || key == KEY_COLON)
    {
        if(edit_digits == 0)
        {
            edit_value = edit_values[edit_field];
        }

        if(!is_field_valid(edit_field, edit_value))
        {
            trigger_beep(200);
            return;
        }

        edit_values[edit_field] = (unsigned char)edit_value;

        if(edit_field < 6)
        {
            edit_field++;
            edit_value = edit_values[edit_field];
            edit_digits = 0;
            show_time_edit_lcd();
        }
        else
        {
            g_rtc.year = edit_values[0];
            g_rtc.month = edit_values[1];
            g_rtc.date = edit_values[2];
            g_rtc.hour = edit_values[3];
            g_rtc.minute = edit_values[4];
            g_rtc.second = edit_values[5];
            g_rtc.week = edit_values[6];
            DS1302_Write(&g_rtc);
            exit_to_clock_mode();
            trigger_beep(120);
        }
    }
}

void handle_alarm_view_key(unsigned char key)
{
    if(key == KEY_ALARM)
    {
        exit_to_clock_mode();
    }
    else if(key == KEY_COLON)
    {
        alarm_view_index++;
        if(alarm_view_index >= ALARM_COUNT)
        {
            alarm_view_index = 0;
        }
        show_alarm_lcd();
    }
    else if(key == KEY_MODE)
    {
        enter_alarm_edit_mode();
    }
    else if(key == KEY_ENTER)
    {
        alarms[alarm_view_index].enable = !alarms[alarm_view_index].enable;
        save_settings_to_eeprom();
        show_alarm_lcd();
        trigger_beep(100);
    }
    else if(key == KEY_BACK)
    {
        alarm_master_enable = !alarm_master_enable;
        save_settings_to_eeprom();
        show_alarm_lcd();
        trigger_beep(100);
    }
}

void handle_alarm_edit_key(unsigned char key)
{
    ALARM_INFO *a;
    unsigned char hh, mm;

    if(key <= 9)
    {
        if(alarm_edit_digits < 4)
        {
            if(alarm_edit_digits == 0)
            {
                alarm_edit_value = 0;
            }
            alarm_edit_value = alarm_edit_value * 10 + key;
            alarm_edit_digits++;
            show_alarm_lcd();
        }
        return;
    }

    if(key == KEY_BACK)
    {
        alarm_edit_value /= 10;
        if(alarm_edit_digits > 0)
        {
            alarm_edit_digits--;
        }
        show_alarm_lcd();
        return;
    }

    if(key == KEY_ENTER)
    {
        hh = alarm_edit_value / 100;
        mm = alarm_edit_value % 100;
        if(hh > 23 || mm > 59)
        {
            trigger_beep(200);
            return;
        }

        a = &alarms[alarm_view_index];
        a->hour = hh;
        a->minute = mm;
        a->enable = 1;
        save_settings_to_eeprom();
        mode = MODE_ALARM_VIEW;
        show_alarm_lcd();
        trigger_beep(120);
    }

    if(key == KEY_ALARM)
    {
        mode = MODE_ALARM_VIEW;
        show_alarm_lcd();
    }
}

void handle_key(unsigned char key)
{
    if(key == KEY_NONE)
    {
        return;
    }

    if(mode == MODE_CLOCK)
    {
        handle_clock_key(key);
    }
    else if(mode == MODE_SET_TIME)
    {
        handle_time_edit_key(key);
    }
    else if(mode == MODE_ALARM_VIEW)
    {
        handle_alarm_view_key(key);
    }
    else
    {
        handle_alarm_edit_key(key);
    }

    update_seg_buffer();
}

void check_chime_and_alarm(void)
{
    unsigned char i;

    if(chime_enable && g_rtc.minute == 0 && g_rtc.second == 0)
    {
        if(last_hourly_hour != g_rtc.hour)
        {
            last_hourly_hour = g_rtc.hour;
            trigger_beep(200);
        }
    }

    if(alarm_master_enable && g_rtc.second == 0)
    {
        for(i = 0; i < ALARM_COUNT; i++)
        {
            if(alarms[i].enable && alarms[i].hour == g_rtc.hour && alarms[i].minute == g_rtc.minute)
            {
                trigger_beep(800);
            }
        }
    }
}

void timer0_isr(void) interrupt 1
{
    TL0 = 0xCD;
    TH0 = 0xD4;

    if(++ms_count % 10 == 0)
    {
        flag_10ms = 1;
    }
    if(ms_count >= 1000)
    {
        ms_count = 0;
        flag_1s = 1;
    }

    if(buzz_ms > 0)
    {
        buzz_ms--;
        buzzer_output(1);
    }
    else
    {
        buzzer_output(0);
    }

    display_scan();
}

void main(void)
{
    unsigned char key;

    hardware_init();
    lcd_init();
    timer0_init();

    load_settings_from_eeprom();
    DS1302_InitDefault();
    DS1302_Read(&g_rtc);

    show_clock_lcd();
    update_seg_buffer();

    while(1)
    {
        if(flag_10ms)
        {
            flag_10ms = 0;
            key = key_scan();
            handle_key(key);
        }

        if(flag_1s)
        {
            flag_1s = 0;
            DS1302_Read(&g_rtc);
            check_chime_and_alarm();

            if(mode == MODE_CLOCK)
            {
                if(!screen_pause)
                {
                    if(++screen_hold_seconds >= 6)
                    {
                        screen_hold_seconds = 0;
                        screen_index++;
                        if(screen_index >= 3)
                        {
                            screen_index = 0;
                        }
                    }
                }
                show_clock_lcd();
            }
            else if(mode == MODE_ALARM_VIEW || mode == MODE_ALARM_EDIT)
            {
                show_alarm_lcd();
            }

            update_seg_buffer();
        }
    }
}
