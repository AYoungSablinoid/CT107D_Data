#include "reg52.h"
#include "intrins.h"

#define DELAY_TIME 5

/** ����I2C����ʱ���ߺ������� */
sbit scl = P2^0;
sbit sda = P2^1;

/**
* @brief I2C������һЩ��Ҫ����ʱ
*
* @param[in] i - ��ʱʱ�����.
* @return none
*/
void i2c_delay(unsigned char i)
{
    do
    {
        _nop_();_nop_();_nop_();_nop_();_nop_();
        _nop_();_nop_();_nop_();_nop_();_nop_();
        _nop_();_nop_();_nop_();_nop_();_nop_();		
    }
    while(i--);        
}

/**
* @brief ����I2C������������.
*
* @param[in] none
* @param[out] none
* @return none
*/
void i2c_start(void)
{
    sda = 1;
    scl = 1;
    i2c_delay(DELAY_TIME);
    sda = 0;
    i2c_delay(DELAY_TIME);
    scl = 0;    
}

/**
* @brief ����I2C����ֹͣ����
*
* @param[in] none
* @param[out] none.
* @return none
*/
void i2c_stop(void)
{
    sda = 0;
    scl = 1;
    i2c_delay(DELAY_TIME);
    sda = 1;
    i2c_delay(DELAY_TIME);       
}

/**
* @brief I2C����һ���ֽڵ�����
*
* @param[in] byt - �����͵��ֽ�
* @return none
*/
void i2c_sendbyte(unsigned char byt)
{
    unsigned char i;
//
	EA = 0;   //�ر��жϣ�������Ϊ�ж϶�Ӱ����д��д��ʱ�򣬵��¶�дʧ�ܡ�
    for(i=0; i<8; i++){
        scl = 0;
        i2c_delay(DELAY_TIME);
        if(byt & 0x80){
            sda = 1;
        }
        else{
            sda = 0;
        }
        i2c_delay(DELAY_TIME);
        scl = 1;
        byt <<= 1;
        i2c_delay(DELAY_TIME);
    }
	EA = 1;
//
    scl = 0;  
}

/**
* @brief �ȴ�Ӧ��
*
* @param[in] none
* @param[out] none
* @return none
*/
unsigned char i2c_waitack(void)
{
	unsigned char ackbit;
	
    scl = 1;
    i2c_delay(DELAY_TIME);
    ackbit = sda; //while(sda);  //wait ack
    scl = 0;
    i2c_delay(DELAY_TIME);
	
	return ackbit;
}

/**
* @brief I2C����һ���ֽ�����
*
* @param[in] none
* @param[out] da
* @return da - ��I2C�����Ͻ��յ�������
*/
unsigned char i2c_receivebyte(void)
{
	unsigned char da = 0;
	unsigned char i;
//
	EA = 0;	
	for(i=0;i<8;i++){   
		scl = 1;
		i2c_delay(DELAY_TIME);
		da <<= 1;
		if(sda) 
			da |= 0x01;
		scl = 0;
		i2c_delay(DELAY_TIME);
	}
	EA = 1;
//
	return da;    
}

/**
* @brief ����Ӧ��
*
* @param[in] ackbit - �趨�Ƿ���Ӧ��
* @return - none
*/
void i2c_sendack(unsigned char ackbit)
{
    scl = 0;
    sda = ackbit;  //0������Ӧ���źţ�1�����ͷ�Ӧ���ź�
    i2c_delay(DELAY_TIME);
    scl = 1;
    i2c_delay(DELAY_TIME);
    scl = 0; 
	sda = 1;
    i2c_delay(DELAY_TIME);
}

/**
* @brief ��д����������һЩ��Ҫ����ʱ
*
* @param[in] i - ָ����ʱʱ��
* @return - none
*/
void operate_delay(unsigned char t)
{
	unsigned char i;
	
	while(t--){
		for(i=0; i<112; i++);
	}
}

/**
* @brief ��AT24C02(add)��д������val
*
* @param[in] add - AT24C02�洢��ַ
* @param[in] val - ��д��AT24C02��Ӧ��ַ������
* @return - none
*/
void write_eeprom(unsigned char add,unsigned char val)
{
    i2c_start();
    i2c_sendbyte(0xa0);
    i2c_waitack();
    i2c_sendbyte(add);
    i2c_waitack();
    i2c_sendbyte(val);
    i2c_waitack();
    i2c_stop();
	operate_delay(10);
}

/**
* @brief ��AT24C02(add)�ж�������da
*
* @param[in] add - AT24C02�洢��ַ
* @param[out] da - ��AT24C02��Ӧ��ַ�ж�ȡ��������
* @return - da
*/
unsigned char read_eeprom(unsigned char add)
{
	unsigned char da;
  
	i2c_start();
	i2c_sendbyte(0xa0);
	i2c_waitack();
	i2c_sendbyte(add);
	i2c_waitack();
	
	i2c_start();
	i2c_sendbyte(0xa1);
	i2c_waitack();
	da = i2c_receivebyte();
	i2c_sendack(1); 
	i2c_stop();
	
	return da;
}
