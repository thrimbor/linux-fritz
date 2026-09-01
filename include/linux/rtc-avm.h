#ifndef _RTC_AVM_H_
#define _RTC_AVM_H_

void *avm_rtc_register_external_interrupt(char *Name);
int avm_rtc_external_interrupt(void *handle, unsigned int interval, unsigned int failed);
int avm_rtc_release_external_interrupt(void *handle);


#endif /*--- #ifndef _RTC_AVM_H_ ---*/
