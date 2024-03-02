
#include <ArduinoFake.h>
#include <unity.h>
#include <stdio.h>


using namespace fakeit;

#include <ArduinoJson.h>

void setUp(void)
{

    //ArduinoFakeReset();
    JsonDocument blub, blab;
}

void tearDown(void) {
    // clean stuff up here
}

void blub_test(void){


            
}


int main()
{



    UNITY_BEGIN();

RUN_TEST(blub_test);

    return UNITY_END();
}


