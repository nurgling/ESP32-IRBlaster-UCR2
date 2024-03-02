// Copyright 2024 Craig Petchell

#ifndef BLASTER_CONFIG_H_
#define BLASTER_CONFIG_H_

// Max pin # is 31 due to 32-bit mask

#ifndef BLASTER_PIN_INDICATOR
#define BLASTER_PIN_INDICATOR 5
#endif

#ifndef BLASTER_PIN_IR_INTERNAL
#define BLASTER_PIN_IR_INTERNAL 12 
#endif

#ifndef BLASTER_PIN_IR_OUT_1
#define BLASTER_PIN_IR_OUT_1 13
#endif

#ifndef BLASTER_PIN_IR_OUT_2
#define BLASTER_PIN_IR_OUT_2 14
#endif

#ifndef BLASTER_ENABLE_IR_LEARN
#define BLASTER_ENABLE_IR_LEARN false
#endif

#if BLASTER_ENABLE_IR_LEARN == true
#ifndef BLASTER_PIN_IR_LEARN
#define BLASTER_PIN_IR_LEARN 5
#endif
#endif

#endif