#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <inc/tm4c1294ncpdt.h>
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/debug.h"
#include "driverlib/sysctl.h"
#include <driverlib/interrupt.h>
#include <driverlib/timer.h>
#include "driverlib/gpio.h"
#include "driverlib/adc.h"
#include "driverlib/pin_map.h"
#include "driverlib/uart.h"

// Settings for ADC read frequency
//define ADC_SAMPLES 8
#define ADC_SAMPLES 100
#define ADC_SAMPLE_RATE 160000

// Settings for the UART
#define UART_SPEED 300000
#define UART_DATABITS UART_CONFIG_WLEN_8
#define UART_STOPBITS UART_CONFIG_STOP_ONE
#define UART_PARITY UART_CONFIG_PAR_NONE

// Predeclarations
void UARTWriteString(char *text);
void Timer0IntHandler();
void ADC0IntHandler();
void ADC1IntHandler();

// Global variables
volatile uint32_t SampleCount1 = 0;
uint32_t Samples1[ADC_SAMPLES][8];  // ADC0 Samples
volatile bool SampleComplete1 = false;
float  Vout;
float  Temp;
uint32_t clockFreq = 0;


int main(void)
{
    uint32_t sampleChnl;
    uint32_t samplePtr;
    char adcSampleValue[150];
   // uint32_t clockFreq = 0;

    // Set the clock
    clockFreq = SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ |
            SYSCTL_OSC_MAIN | SYSCTL_USE_PLL |
            SYSCTL_CFG_VCO_480), 120000000);

    // Clear the samples register
    for (samplePtr = 0; samplePtr < ADC_SAMPLES; samplePtr++) {
        for (sampleChnl = 0; sampleChnl < 8; sampleChnl++) {
            Samples1[samplePtr][sampleChnl] = 0;
        }
    }

    // Prepare the Timer
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
    SysCtlDelay(3);
    TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);
    TimerLoadSet(TIMER0_BASE, TIMER_A, (clockFreq/ADC_SAMPLE_RATE) - 1);

    // Prepare the Port A pins (UART0)
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    SysCtlDelay(3);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0|GPIO_PIN_1);


    // Prepare the Port E pins (ADC)
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
    SysCtlDelay(3);
    GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_0);

    // Prepare UART0 for ADC samples display
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
    SysCtlDelay(3);
    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);
    UARTConfigSetExpClk(UART0_BASE, clockFreq, UART_SPEED,
        (UART_DATABITS | UART_STOPBITS | UART_PARITY));


    // Prepare ADC0
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);
    SysCtlDelay(3);
    ADCReferenceSet(ADC0_BASE, ADC_REF_INT);
    ADCSequenceConfigure(ADC0_BASE, 0, ADC_TRIGGER_PROCESSOR, 0);
    ADCSequenceStepConfigure(ADC0_BASE, 0, 0, ADC_CTL_CH3|ADC_CTL_IE|ADC_CTL_END);
    ADCSequenceEnable(ADC0_BASE, 0);

    // Setup the Timer Interrupt
    IntRegister(INT_TIMER0A, Timer0IntHandler);
    SysCtlDelay(3);
    IntEnable(INT_TIMER0A);
    TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);

    // Setup the ADC0 Interrupt
    IntRegister(INT_ADC0SS0, ADC0IntHandler);
    SysCtlDelay(3);
    IntEnable(INT_ADC0SS0);
    ADCIntEnable(ADC0_BASE, 0);

    // Enable the Master Interrupt
    IntMasterEnable();

    // Start the timer
    TimerEnable(TIMER0_BASE, TIMER_A);
    IntEnable(INT_UART0);
    UARTIntEnable(UART0_BASE, UART_INT_RX | UART_INT_RT);


    while (1) {
        if ((SampleComplete1 == true)) {
            // Loop through the Samples array and convert
            // the ADC value to a string and send to UART0
            for (samplePtr = 0; samplePtr < ADC_SAMPLES; samplePtr++) {
                Vout = (Samples1[samplePtr][0]*3.3)/4096;
                Temp = (184.61*Vout)-229.939;
                sprintf(adcSampleValue, "Temperature Reading %i: %f degrees C\r\n",(samplePtr+1),
                       Temp);
                UARTWriteString(adcSampleValue);
            }
            SampleComplete1 = false;
            while (1) {};
        }
    }
}

// Accepts a null terminated string and writes the string to UART0
void UARTWriteString(char *text) {
    uint32_t ptr = 0;

    while (text[ptr] != 0) {
        UARTCharPut(UART0_BASE, text[ptr++]);
    }
}

// Timer interrupt handler
void Timer0IntHandler() {
    // Clear the interrupt
    TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);

    ADCProcessorTrigger(ADC0_BASE, 0);                  // Request ADC0 Process
 }

void ADC0IntHandler() {
    // Clear the interrupt
    ADCIntClear(ADC0_BASE, 0);

    // Retrieve the ADC value
    ADCSequenceDataGet(ADC0_BASE, 0, Samples1[SampleCount1++]);
    // Stop sampling if the count has been reached.
    if (SampleCount1 >= ADC_SAMPLES) {
        TimerDisable(TIMER0_BASE, TIMER_A);
        SampleComplete1 = true;
    }
}


