# ESP32 IR blaster for the Unfolded Circle Remote Two.

This project implements the [Unfolded Circle Remote Two](https://www.unfoldedcircle.com/) dock API to add additional IR blasters
to your remote setup.

This allows the installation of an IR blaster in an independant location from where your
remote and charging dock are located. For example, in an equipment rack or cupboard or for
multi-room setups.

## Requirements

1. An [Unfolded Circle Remote Two](https://www.unfoldedcircle.com/).
1. An ESP32 dev board
1. Platform.io to build and upload firmware
1. Some basic soldering / breadboarding skills and basic understanding of electronic.
1. Some basic electronic parts and IR led.

## Getting Started

1. Add your specific board to ``platformio.ini``. Feel free to contribute this back as a pull-request. 
1. Modify pin mappings, if required in ``lib/config/blaster_config.h`` or by setting `-D defines` in ``build_flags`` respectively (see section [Customizing Pinout/Configuration](#customizing-pinout-configuration)).
1. For enabling the *learning of IR codes* you have to add ``-DBLASTER_ENABLE_IR_LEARN=true`` to the build_flags of your used Platform.io env and check if the assigned GPIO pin is right.
1. IR learning is still under development and tested only with some basic IR codes

## Customizing Pinout/Configuration 

The following defines are available in ``lib/config/blaster_config.h`` and can be overwritten by setting ``build_flags`` in ``platformio.ini``.

| Define | Description | Options |
|:-------|:------------|:--------|
|`BLASTER_INDICATOR_MODE` | Defines the type of indicator led used for custom dock | `INDICATOR_OFF` no indicator led available<br/>`INDICATOR_LED` single color led (__default__)<br/>`INDICATOR_PIXEL` digital RGB neopixel led |
|`BLASTER_PIN_INDICATOR` | Defines GPIO pin where indicator led is connected to  | Default: `5`<br/>Has no effect if `BLASTER_INDICATOR_MODE=INDICATOR_OFF` |
|`BLASTER_ENABLE_IR_INTERNAL` | Enables/Disables internal IR channel of dock | `true` channel is enabled (__default__)<br/> `false` channel is disabled |
|`BLASTER_PIN_IR_INTERNAL` | Defines GPIO pin of internal IR channel | Default: `12`<br/>Has no effect if `BLASTER_ENABLE_IR_INTERNAL=false` |
|`BLASTER_ENABLE_IR_OUT_1` | Enables/Disables external IR channel 1 of dock | `true` channel is enabled (__default__)<br/> `false` channel is disabled |
|`BLASTER_PIN_IR_OUT_1` | Defines GPIO pin of external IR channel 1 | Default: `13`<br/>Has no effect if `BLASTER_ENABLE_IR_OUT_1=false` |
|`BLASTER_ENABLE_IR_OUT_2` | Enables/Disables external IR channel 2 of dock | `true` channel is enabled (__default__)<br/> `false` channel is disabled |
|`BLASTER_PIN_IR_OUT_2` | Defines GPIO pin of external IR channel 2 | Default: `14`<br/>Has no effect if `BLASTER_ENABLE_IR_OUT_2=false` |
|`BLASTER_ENABLE_IR_LEARN` | Enables/Disables learning of IR codes by an IR receiver | `true` learning is enabled<br/> `false` learning is disabled  (__default__)|
|`BLASTER_PIN_IR_LEARN` | Defines GPIO pin IR receiver is connected to | Default: `16`<br/>Has no effect if `BLASTER_ENABLE_IR_LEARN=false` |
|`BLASTER_ENABLE_ETH` | Enables/Disables an wired Ethernet interface<br/>Currently only Olimex Boards supported. | `true` Ethernet interface available. Requires definition of `OLIMEX_ESP`<br/> `false` Ethernet interface not available  (__default__)|
|`OLIMEX_ESP` | Defines ESP32 package used in Olimex POE Board<br/>Impacts the pins for ethernet interface | `WROOM` tested and confirmed working<br/>`WROVER` untested ⚠️, therefore breaks build on purpose. Adaption in `eth_service.cpp` required!<br/>Has no effect if `BLASTER_ENABLE_ETH=false` |




# Electronics

## KoeBlaster Hardware

KoeBlaster is designed to be a drop-in replacement of the Logitech Harmony Hub PCB working with Remote Two.

⚠️ Schematic is currently under review. Comments are apreciated!
PCB is not yet manufactured.

### Schematic

![Serving suggestion](https://github.com/itcorner/ESP32-IRBlaster-UCR2/blob/main/kicad/KoeBlaster/exports/schematic_v0.1.png?raw=true)

### Board

#### Top

![Serving suggestion](https://github.com/itcorner/ESP32-IRBlaster-UCR2/blob/main/kicad/KoeBlaster/exports/KoeBlaster-brd_top_elements.svg?raw=true)

![Serving suggestion](https://github.com/itcorner/ESP32-IRBlaster-UCR2/blob/main/kicad/KoeBlaster/exports/KoeBlaster-brd_top.svg?raw=true)

#### Bottom

![Serving suggestion](https://github.com/itcorner/ESP32-IRBlaster-UCR2/blob/main/kicad/KoeBlaster/exports/KoeBlaster-brd_bottom_elements.svg?raw=true)

![Serving suggestion](https://github.com/itcorner/ESP32-IRBlaster-UCR2/blob/main/kicad/KoeBlaster/exports/KoeBlaster-brd_bottom.svg?raw=true)



## DIY Breadboard Example

At least one IR led + transistors and resistors is required for this to operate correctly.
IR learning was tested with an CHQ1838 Infrared receiver module.

Schematic (⚠️ needs update for IR learning!):

![Serving suggestion](https://github.com/itcorner/ESP32-IRBlaster-UCR2/blob/main/kicad/schematic.png?raw=true)

# Integrating with the remote

In the remote web interface:

1. Select *Integrations & docks*
1. Under *Docks* click *+*
1. Click on *Discover Docks*
1. Dock should be recognized either via Bluetooth or via WiFi/Ethernet
1. If recognized via Bluetooth, you have to mandatorily specify Wifi settings during adoption process.
1. Click *Next*
1. The blaster should be added as a new dock. If there were issues, then download the recent log files in
*Settings* / *Development* / *Logs*. There should be a reason for any failures logged by the remote.

## Caveats

The Unfolded Circle Remote Two API, while [documented](https://github.com/unfoldedcircle/core-api/blob/main/dock-api/README.md),
the API could change with a remote update before this library is updated. 

This project is not from Unfolded Circle, so do not ask for support there.

This is just a minimal implementation, and doesn't not support OTA updates, web config, etc at this stage.

The Unfolded Circle web interface will report an error when attempting to check for firmware updates. This is normal as
the remote does not know about this particular dock.
