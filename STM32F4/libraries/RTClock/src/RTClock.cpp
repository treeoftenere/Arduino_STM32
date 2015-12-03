/******************************************************************************
 * The MIT License
 *
 * Copyright (c) 2010 LeafLabs LLC.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *****************************************************************************/

/**
 Inspired of the F1xx version adapted for the F4xx, not much F1xx left.
 author : Martin Ayotte, 2015.
 */

#include "RTClock.h"

static rtc_dev rtc = {
    .regs         = RTC_BASE,
//    .handlers     = { [NR_RTC_HANDLERS - 1] = 0 },
};

rtc_dev *RTC = &rtc;


RTClock::RTClock() {
    RTClock(RTCSEL_HSE, 7999, 124);
}

RTClock::RTClock(rtc_clk_src src) {
    RTClock(src, 0, 0);	
}

RTClock::RTClock(rtc_clk_src src, uint16 sync_prescaler, uint16 async_prescaler) {
    uint32 t = 0;
    RCC_BASE->APB1ENR |= RCC_APB1RSTR_PWRRST;
    dbg_printf("RCC_BASE->APB1ENR = %08X\r\n", RCC_BASE->APB1ENR);
    dbg_printf("before bkp_init\r\n");
    bkp_init();		// turn on peripheral clocks to PWR and BKP and reset the backup domain via RCC registers.
                        // (we reset the backup domain here because we must in order to change the rtc clock source).
    dbg_printf("before bkp_disable_writes\r\n");
    bkp_disable_writes();
    dbg_printf("before bkp_enable_writes\r\n");
    bkp_enable_writes();	// enable writes to the backup registers and the RTC registers via the DBP bit in the PWR control register
    dbg_printf("RCC_BASE->CFGR = %08X\r\n", RCC_BASE->CFGR);
    RCC_BASE->CFGR |= (0x08 << 16); // Set the RTCPRE to HSE / 8.
    dbg_printf("RCC_BASE->CFGR = %08X\r\n", RCC_BASE->CFGR);

    switch (src) {	
        case RTCSEL_LSE :
	    dbg_printf("Preparing RTC for LSE mode\r\n");
	    if ((RCC_BASE->BDCR & 0x00000300) != 0x0100)
                RCC_BASE->BDCR = 0x00010000; // Reset the entire Backup domain
            RCC_BASE->BDCR = 0x00008101;
            dbg_printf("RCC_BASE->BDCR = %08X\r\n", RCC_BASE->BDCR);
            while (!(RCC_BASE->BDCR & 0x00000002)) {
                if (++t > 1000000) {
                    dbg_printf("RCC_BASE->BDCR.LSERDY Timeout !\r\n");
                    dbg_printf("RCC_BASE->BDCR = %08X\r\n", RCC_BASE->BDCR);
                    return;
                }
            }
            dbg_printf("RCC_BASE->BDCR = %08X\r\n", RCC_BASE->BDCR);
            rtc_enter_config_mode();
            if (sync_prescaler == 0 && async_prescaler == 0)
                RTC_BASE->PRER = 255 | (127 << 16);
            else
                RTC_BASE->PRER = sync_prescaler | (async_prescaler << 16);
	    break;
	case RTCSEL_LSI :
	    dbg_printf("Preparing RTC for LSI mode\r\n");
	    if ((RCC_BASE->BDCR & 0x00000300) != 0x0200)
                RCC_BASE->BDCR = 0x00010000; // Reset the entire Backup domain
            RCC_BASE->BDCR = 0x00008204;
            RCC_BASE->CSR |= 0x00000001;
            dbg_printf("RCC_BASE->BDCR = %08X\r\n", RCC_BASE->BDCR);
            while (!(RCC_BASE->CSR & 0x00000002)) {
                if (++t > 1000000) {
                    dbg_printf("RCC_BASE->CSR.LSIRDY Timeout !\r\n");
                    dbg_printf("RCC_BASE->CSR = %08X\r\n", RCC_BASE->CSR);
                    return;
                }
            }
            dbg_printf("RCC_BASE->BDCR = %08X\r\n", RCC_BASE->BDCR);
       	    rtc_enter_config_mode();
            if (sync_prescaler == 0 && async_prescaler == 0)
               	RTC_BASE->PRER = 249 | (127 << 16);
            else
               	RTC_BASE->PRER = sync_prescaler | (async_prescaler << 16);
	    break;
	case RTCSEL_DEFAULT: 
	case RTCSEL_HSE : 
	    dbg_printf("Preparing RTC for HSE mode\r\n");
	    if ((RCC_BASE->BDCR & 0x00000300) != 0x0300)
                RCC_BASE->BDCR = 0x00010000; // Reset the entire Backup domain
            RCC_BASE->BDCR = 0x00008304;
            dbg_printf("RCC_BASE->BDCR = %08X\r\n", RCC_BASE->BDCR);
            rtc_enter_config_mode();
            if (sync_prescaler == 0 && async_prescaler == 0)
                RTC_BASE->PRER = 7999 | (124 << 16);
            else
                RTC_BASE->PRER = sync_prescaler | (async_prescaler << 16);
	    break;
	case RTCSEL_NONE:
	    dbg_printf("Preparing RTC for NONE mode\r\n");
	    if ((RCC_BASE->BDCR & 0x00000300) != 0x0000)
                RCC_BASE->BDCR = 0x00010000; // Reset the entire Backup domain
	    RCC_BASE->BDCR = RCC_BDCR_RTCSEL_NONE;
	    //do nothing. Have a look at the clocks to see the diff between NONE and DEFAULT
	    break;
    }
    RCC_BASE->CR |= 0x00000040; // Turn to 24hrs mode
//    dbg_printf("before rtc_clear_sync\r\n");
//    rtc_clear_sync();
//    dbg_printf("before rtc_wait_sync\r\n");
//    rtc_wait_sync();
    rtc_exit_config_mode();
    dbg_printf("end of rtc_init\r\n");
}

/*
RTClock::~RTClock() {
    //to implement
}
*/	
	
void RTClock::setTime (time_t time_stamp) {
    unsigned char years = 0;
    unsigned char months = 0;
    unsigned char monthLength = 0;
    unsigned char wdays = 0;
    unsigned char hours = 0;
    unsigned char mins = 0;
    unsigned char secs = 0;
    unsigned long days;

    secs = time_stamp % 60;
    time_stamp /= 60; // now it is minutes
    mins = time_stamp % 60;
    time_stamp /= 60; // now it is hours
    hours = time_stamp % 24;
    time_stamp /= 24; // now it is days
    wdays = ((time_stamp + 4) % 7) + 1;  // Sunday is day 1 
  
    while((unsigned)(days += (LEAP_YEAR(years) ? 366 : 365)) <= time_stamp) {
        years++;
    }
  
    days -= LEAP_YEAR(years) ? 366 : 365;
    time_stamp -= days; // now it is days in this year, starting at 0
  
    for (months = 0; months < 12; months++) {
        if (months == 1) { // february
            if (LEAP_YEAR(years)) {
                monthLength = 29;
            } else {
                monthLength = 28;
            }
        } else {
            monthLength = monthDays[months];
        }
    
        if (time_stamp >= monthLength) {
            time_stamp -= monthLength;
        } else {
            break;
        }
    }
    months++;  // jan is month 1  
    days = time_stamp + 1;     // day of month
    rtc_enter_config_mode();
    RTC_BASE->TR = ((hours / 10) << 20) | ((hours % 10) << 16) | ((mins / 10) << 12) | ((mins % 10) << 8) | ((secs / 10) << 4) | (secs % 10);
    RTC_BASE->DR = ((years / 10) << 20) | ((years % 10) << 16) | (wdays << 13) | ((months / 10) << 12) | ((months % 10) << 8) |	((days / 10) << 4) | (days % 10);
    rtc_exit_config_mode();		                
}
	
void RTClock::setTime (struct tm* tm_ptr) {
    rtc_enter_config_mode();
    RTC_BASE->TR = ((tm_ptr->tm_hour / 10) << 20) | ((tm_ptr->tm_hour % 10) << 16) | 
			((tm_ptr->tm_min / 10) << 12) | ((tm_ptr->tm_min % 10) << 8) | 
			((tm_ptr->tm_sec / 10) << 4) | (tm_ptr->tm_sec % 10);
    RTC_BASE->DR = ((tm_ptr->tm_year / 10) << 20) | ((tm_ptr->tm_year % 10) << 16) | (tm_ptr->tm_wday << 13) | 
			((tm_ptr->tm_mon / 10) << 12) | ((tm_ptr->tm_mon % 10) << 8) |
			((tm_ptr->tm_mday / 10) << 4) | (tm_ptr->tm_mday % 10);
    rtc_exit_config_mode();		                
}
	
time_t RTClock::getTime() {
    int years = 10 * ((RTC_BASE->DR & 0x00F00000) >> 20) + ((RTC_BASE->DR & 0x000F0000) >> 16);
    int months = 10 * ((RTC_BASE->DR & 0x00001000) >> 12) + ((RTC_BASE->DR & 0x00000F00) >> 8);
    int days = 10 * ((RTC_BASE->DR & 0x00000030) >> 4) + (RTC_BASE->DR & 0x000000F);
    int hours = 10 * ((RTC_BASE->TR & 0x00300000) >> 20) + ((RTC_BASE->TR & 0x000F0000) >> 16);
    int mins = 10 * ((RTC_BASE->TR & 0x00007000) >> 12) + ((RTC_BASE->TR & 0x0000F00) >> 8);
    int secs = 10 * ((RTC_BASE->TR & 0x00000070) >> 4) + (RTC_BASE->TR & 0x0000000F);
    // seconds from 1970 till 1 jan 00:00:00 of the given year
    time_t t = (years + 30) * SECS_PER_DAY * 365;
    for (int i = 0; i < years; i++) {
	if (LEAP_YEAR(i)) {
	    t +=  SECS_PER_DAY;   // add extra days for leap years
	}
    }
    // add days for this year, months start from 1
    for (int i = 1; i < months; i++) {
	if ( (i == 2) && LEAP_YEAR(years)) { 
		    t += SECS_PER_DAY * 29;
	} else {
		    t += SECS_PER_DAY * monthDays[i - 1];  //monthDays array starts from 0
	}
    }
    t += (days - 1) * SECS_PER_DAY + hours * SECS_PER_HOUR + mins * SECS_PER_MIN + secs;
    return t;
}
	
struct tm* RTClock::getTime(struct tm* tm_ptr) {
    tm_ptr->tm_year = 10 * ((RTC_BASE->DR & 0x00F00000) >> 20) + ((RTC_BASE->DR & 0x000F0000) >> 16);
    tm_ptr->tm_mon = 10 * ((RTC_BASE->DR & 0x00001000) >> 12) + ((RTC_BASE->DR & 0x00000F00) >> 8);
    tm_ptr->tm_mday = 10 * ((RTC_BASE->DR & 0x00000030) >> 4) + (RTC_BASE->DR & 0x000000F);
    tm_ptr->tm_hour = 10 * ((RTC_BASE->TR & 0x00300000) >> 20) + ((RTC_BASE->TR & 0x000F0000) >> 16);
    tm_ptr->tm_min = 10 * ((RTC_BASE->TR & 0x00007000) >> 12) + ((RTC_BASE->TR & 0x0000F00) >> 8);
    tm_ptr->tm_sec = 10 * ((RTC_BASE->TR & 0x00000070) >> 4) + (RTC_BASE->TR & 0x0000000F);
    return tm_ptr;
}
	
void RTClock::createAlarm(voidFuncPtr function, time_t alarm_time_t) {
//    rtc_set_alarm(alarm_time_t); //must be int... for standardization sake. 
//    rtc_attach_interrupt(RTC_ALARM_SPECIFIC_INTERRUPT, function);
}
	
void RTClock::createAlarm(voidFuncPtr function, tm* alarm_tm) {
//    time_t alarm = mktime(alarm_tm);//convert to time_t
//    createAlarm(function, alarm);	
}

void RTClock::attachSecondsInterrupt(voidFuncPtr function) {
//    rtc_attach_interrupt(RTC_SECONDS_INTERRUPT, function);
}

void RTClock::detachSecondsInterrupt() {
//    rtc_detach_interrupt(RTC_SECONDS_INTERRUPT);
}

void RTClock::setAlarmATime (tm * tm_ptr, bool hours_match, bool mins_match, bool secs_match, bool date_match) {
    rtc_enter_config_mode();
    unsigned int bits = ((tm_ptr->tm_mday / 10) << 28) | ((tm_ptr->tm_mday % 10) << 24) |
		        ((tm_ptr->tm_hour / 10) << 20) | ((tm_ptr->tm_hour % 10) << 16) | 
			((tm_ptr->tm_min / 10) << 12) | ((tm_ptr->tm_min % 10) << 8) | 
			((tm_ptr->tm_sec / 10) << 4) | (tm_ptr->tm_sec % 10);
    if (!date_match) bits |= (1 << 31);
    if (!hours_match) bits |= (1 << 23);
    if (!mins_match) bits |= (1 << 15);
    if (!secs_match) bits |= (1 << 7);
    RTC_BASE->ALRMAR = bits;
    RTC_BASE->CR |= (1 << RTC_CR_ALRAIE_BIT); // turn on ALRAIE
    rtc_exit_config_mode();
}


void RTClock::setAlarmATime (time_t alarm_time, bool hours_match, bool mins_match, bool secs_match, bool date_match) {	
    struct tm* tm_ptr = gmtime(&alarm_time);
    setAlarmATime(tm_ptr, hours_match, mins_match, secs_match, date_match);
}


void RTClock::turnOffAlarmA() {
    rtc_enter_config_mode();
    RTC_BASE->CR &= ~(1 << RTC_CR_ALRAIE_BIT); // turn off ALRAIE
    rtc_exit_config_mode();
}


void RTClock::setAlarmBTime (tm * tm_ptr, bool hours_match, bool mins_match, bool secs_match, bool date_match) {
    rtc_enter_config_mode();
    unsigned int bits = ((tm_ptr->tm_mday / 10) << 28) | ((tm_ptr->tm_mday % 10) << 24) |
		        ((tm_ptr->tm_hour / 10) << 20) | ((tm_ptr->tm_hour % 10) << 16) | 
			((tm_ptr->tm_min / 10) << 12) | ((tm_ptr->tm_min % 10) << 8) | 
			((tm_ptr->tm_sec / 10) << 4) | (tm_ptr->tm_sec % 10);
    if (!date_match) bits |= (1 << 31);
    if (!hours_match) bits |= (1 << 23);
    if (!mins_match) bits |= (1 << 15);
    if (!secs_match) bits |= (1 << 7);
    RTC_BASE->ALRMBR = bits;
    RTC_BASE->CR |= (1 << RTC_CR_ALRBIE_BIT); // turn on ALRBIE
    rtc_exit_config_mode();
}


void RTClock::setAlarmBTime (time_t alarm_time, bool hours_match, bool mins_match, bool secs_match, bool date_match) {	
    struct tm* tm_ptr = gmtime(&alarm_time);
    setAlarmBTime(tm_ptr, hours_match, mins_match, secs_match, date_match);
}


void RTClock::turnOffAlarmB() {
    rtc_enter_config_mode();
    RTC_BASE->CR &= ~(1 << RTC_CR_ALRBIE_BIT); // turn off ALRBIE
    rtc_exit_config_mode();
}


void RTClock::setPeriodicWakeup(uint16 period) {
    rtc_enter_config_mode();
    dbg_printf("before setting RTC_BASE->WUTR\r\n");    
    RTC_BASE->WUTR = period; // set the period
    dbg_printf("before setting RTC_BASE->CR.WUCKSEL\r\n");    
    RTC_BASE->CR &= ~(3); RTC_BASE->CR |= 4; // Set the WUCKSEL to 1Hz (0x00000004)
    if (period == 0)
        RTC_BASE->CR &= ~(1 << RTC_CR_WUTIE_BIT); // if period is 0, turn off periodic wakeup interrupt.
    else {
        dbg_printf("before turn ON RTC_BASE->CR.WUTIE\r\n");    
        RTC_BASE->CR |= (1 << RTC_CR_WUTIE_BIT); // turn on WUTIE
    }
    dbg_printf("RCC_BASE->CR = %08X\r\n", RCC_BASE->CR);
    rtc_exit_config_mode();
    rtc_enable_wakeup_event();
    nvic_irq_enable(NVIC_RTC);
    nvic_irq_enable(NVIC_RTCALARM);
    dbg_printf("setPeriodicWakeup() done !\r\n");
}

extern "C" {
void __irq_rtc(void) {
    dbg_printf("__irq_rtc() called !\r\n");
    *bb_perip(&EXTI_BASE->PR, EXTI_RTC_WAKEUP_BIT) = 1;
}
void __irq_rtcalarm(void) {
    dbg_printf("__irq_rtcalarm() called !\r\n");
    *bb_perip(&EXTI_BASE->PR, EXTI_RTC_ALARM_BIT) = 1;
}
}