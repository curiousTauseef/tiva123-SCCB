#include"sccb.h"

#include<stdlib.h>
#include<stdio.h>
#include<driverlib/timer.h>
#include<driverlib/gpio.h>
#include<inc/hw_sysctl.h>
#include<driverlib/sysctl.h>

struct sccbCamContext camera;

uint32_t data;
volatile int writting=0;
volatile int reading=0;
volatile int busy = 0;

void saveCamera(uint32_t gpioBaseClk, uint32_t gpioPinClk, uint32_t gpioBaseData, uint32_t gpioPinData)
{
     camera.gpioClockPort = gpioBaseClk;
     camera.gpioClockPin = gpioPinClk;
     camera.gpioDataPort = gpioBaseData;
     camera.gpioDataPin = gpioPinData;
}

/*
 * initalizeSCCB(gpioBaseClk, gpioPinClk, gpioBaseData, gpioPinData)
 *  gpioBaseClk:   The base address of the gpio port to output the clock signal.
 *  gpioPinClk:    The pin MACRO for the selected gpio base clock signal.
 *  gpioBaseData:  The base address of the gpio port to output/input the data.
 *  gpioPinData:   The pin MACRO for the selected gpio base data signal.
 *
 * Description: Sets up the necessary gpio and starts the timers to clock the signal.
 */
void initalizeSCCB(uint32_t gpioBaseClk, uint32_t gpioPinClk, uint32_t gpioBaseData, uint32_t gpioPinData)
{
    saveCamera(gpioBaseClk, gpioPinClk, gpioBaseData, gpioPinData);
    if(gpioBaseClk==GPIO_PORTA_BASE || gpioBaseData==GPIO_PORTA_BASE) SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    if(gpioBaseClk==GPIO_PORTB_BASE || gpioBaseData==GPIO_PORTB_BASE) SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
    if(gpioBaseClk==GPIO_PORTC_BASE || gpioBaseData==GPIO_PORTC_BASE) SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOC);
    if(gpioBaseClk==GPIO_PORTD_BASE || gpioBaseData==GPIO_PORTD_BASE) SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
    if(gpioBaseClk==GPIO_PORTE_BASE || gpioBaseData==GPIO_PORTE_BASE) SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
    if(gpioBaseClk==GPIO_PORTF_BASE || gpioBaseData==GPIO_PORTF_BASE) SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);

    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER1);
    TimerDisable(TIMER1_BASE, TIMER_A);
    TimerConfigure(TIMER1_BASE, TIMER_CFG_SPLIT_PAIR | TIMER_CFG_A_PERIODIC);
    TimerLoadSet(TIMER1_BASE, TIMER_A, SysCtlClockGet()/(SCCBCLOCKRATE*2));
    TimerIntDisable(TIMER1_BASE, TIMER_TIMA_TIMEOUT);
    TimerIntClear(TIMER1_BASE, TIMER_TIMA_TIMEOUT);
    TimerIntRegister(TIMER1_BASE, TIMER_A, TimerAInterupt);
    TimerIntEnable(TIMER1_BASE, TIMER_TIMA_TIMEOUT);


}

void sccbWrite(uint8_t addr, uint8_t reg, uint8_t information)
{
    writting=1;
    data=(addr<<18)+(reg<<9)+information; //Rolling left 9 bits to account for the don't care.
    TimerEnable(TIMER1_BASE, TIMER_A);
    while(writting == 1 || busy==1);
}

void TimerAInterupt(void)
{
    static int clockCounter = -2;
    static int bitCounter = 0;
    static int stop = 0;


    if(writting == 1 && clockCounter >= 0)
    { //3-Phase write
            if((clockCounter%2) == 1)
            {
                //Toggle the clock signal
                if(clockCounter == 1) GPIOPinWrite(camera.gpioClockPort, camera.gpioClockPin, 1);
                else GPIOPinWrite(camera.gpioClockPort, camera.gpioClockPin, 0);
            }

            else if((clockCounter % 4) == 0)
            {
                GPIOPinWrite(camera.gpioDataPort, camera.gpioDataPin, (data>>(26 - bitCounter)) & 0b1);
                bitCounter++;
            }

            if(bitCounter==27) //It sent 27 bits Stop the sequence
            {
                writting=0;
                stop=1;
                bitCounter=0;
                clockCounter=0;
                TimerIntClear(TIMER1_BASE, TIMER_TIMA_TIMEOUT);
                return;

            }
            else
            {
                clockCounter++;
                if(clockCounter == 4) clockCounter = 0;
            }
    }

    if(reading == 1 && clockCounter >=0)
    {
        if((clockCounter%2) == 1)
        {
            //Toggle the clock signal
            if(clockCounter == 1) GPIOPinWrite(camera.gpioClockPort, camera.gpioClockPin, 1);
            else GPIOPinWrite(camera.gpioClockPort, camera.gpioClockPin, 0);
        }

        clockCounter++;
    }

    if(stop == 1)
    {
        //Stop sequence
        if(clockCounter==0) GPIOPinWrite(camera.gpioDataPort, camera.gpioDataPin, 0);

        if(clockCounter == 1) GPIOPinWrite(camera.gpioClockPort, camera.gpioClockPin, 1);

        if(clockCounter == 2)
        {
            GPIOPinWrite(camera.gpioDataPort, camera.gpioDataPin, 1);
            stop = 0;
            clockCounter = -3;
            TimerDisable(TIMER1_BASE, TIMER_A);
            busy = 0;

        }
        clockCounter++;


    }

    if(clockCounter < 0)
    {
        //Start sequence
        if(reading==0 && writting==0)
        {
            TimerIntClear(TIMER1_BASE, TIMER_TIMA_TIMEOUT);
            return;
        }

        busy = 1;
        if((clockCounter%2) == 1) GPIOPinWrite(camera.gpioClockPort, camera.gpioClockPin, 0);
        else GPIOPinWrite(camera.gpioDataPort, camera.gpioDataPin, 0);

        clockCounter++;

    }
    TimerIntClear(TIMER1_BASE, TIMER_TIMA_TIMEOUT);
}
