// Copyright 2024 Craig Petchell

#ifndef BLASTER_CONFIG_H_
#define BLASTER_CONFIG_H_

// Max pin # is 31 due to 32-bit mask


#ifndef BLASTER_INDICATOR_MODE
#define BLASTER_INDICATOR_MODE INDICATOR_LED
#endif

#ifndef BLASTER_PIN_INDICATOR
#define BLASTER_PIN_INDICATOR 5
#endif


#ifndef BLASTER_ENABLE_IR_INTERNAL
#define BLASTER_ENABLE_IR_INTERNAL true
#endif

#ifndef BLASTER_PIN_IR_INTERNAL
#define BLASTER_PIN_IR_INTERNAL 12 
#endif


#ifndef BLASTER_ENABLE_IR_OUT_1
#define BLASTER_ENABLE_IR_OUT_1 true
#endif

#ifndef BLASTER_PIN_IR_OUT_1
#define BLASTER_PIN_IR_OUT_1 13
#endif


#ifndef BLASTER_ENABLE_IR_OUT_2
#define BLASTER_ENABLE_IR_OUT_2 true
#endif

#ifndef BLASTER_PIN_IR_OUT_2
#define BLASTER_PIN_IR_OUT_2 14
#endif


#ifndef BLASTER_ENABLE_IR_LEARN
#define BLASTER_ENABLE_IR_LEARN false
#endif

#if BLASTER_ENABLE_IR_LEARN == true
#ifndef BLASTER_PIN_IR_LEARN
#define BLASTER_PIN_IR_LEARN 16
#endif
#endif

#ifndef BLASTER_ENABLE_RESETBTN
#define BLASTER_ENABLE_RESETBTN false
#endif

#if BLASTER_ENABLE_RESETBTN == true
#ifndef BLASTER_PIN_RESETBTN
#define BLASTER_PIN_RESETBTN 23
#endif
#endif

#ifndef BLASTER_ENABLE_ETH
#define BLASTER_ENABLE_ETH false
#endif

#ifndef BLASTER_ENABLE_OTA
#define BLASTER_ENABLE_OTA false
#endif



#endif