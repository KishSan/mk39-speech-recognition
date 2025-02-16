# MK39 Offline Voice Command Control System

## Overview

This repository contains code build off the ESP-SKAINET framework which was designed specifically to be used on the following boards:
 * ESP32-S3-Korvo-1
 * ESP-BOX
 * ESP-S3-Korvo-2
 * ESP32-S3-EYE
 * ESP32-P4-Function-EV

The GitHub repository can be found here:
* [ESP-Skainet Framework](https://github.com/espressif/esp-skainet)

Modifications were needed to the framework to work with DevKit modules. Special thanks to this repository for getting this to work with the Development modules:
* [ESP-Skainet framework modification for ESP32-S3 DevKitC](https://github.com/0015/esp-skainet/tree/ESP32-S3-Devkit-C)

Due to the nature of the project, the framework was stripped down to only contain necessary modules for this application. Features including:
* English speech recognition
* Servo control using LEDC
* Addressable LED control using RMT drivers

The final revision of this code base works with the ESP32-S3 N8R8 module. Different variations will need to be configured in the SDK using the menuconfig option.

### Additional Hardware Required

* INMP 441 MEMS Microphone
    * This addition is reaquired for the speech input when using the DevKit modules

### Setting up the ESP-IDF environment
Development for this project was done with the ESP-IDF v5.0.8 extension in VSCode. Testing was done on later versions but there are compatibality issues with some of the modules that I have not resolved yet. 

Further details on setting up the IDF environment can be found here and is outside the scope of this documentation.
* [ESP-IDF Guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html)

Once the environment has been configured and setup properly you can test by opening an ESP-IDF terminal. This can be accessed from the extension on the left in VSCode, which will open a terminal and run the export script to set environment variables among other setting needed for development.

### Configure, Build and Flash

Once the ESP-IDF development environment has been set up, we can begin configuring and flashing the project.
##### set-target 

This is important to compile the proper binaries for the device you are using. Since the sdkconfig file included in this repo is specific for the esp32s3, it should work out of the box.
```
idf.py set-target esp32s3
```

##### configure

The sdkconfig file contains the following command activations and phrases:

* Activation: Jarvis (wn9_jarvis_tts)
* Commmands (not case-sensitive):
    * Wake Up (WdK cP)
    * Hulk Out (hcLK tT)
    * Let's Go (LfTS Gb)

Manual SDK changes can be done through menuconfig:
```
idf.py menuconfig
```

To reset the framework configurations to the default configuration for the ESP32-S3 module, use the following command example to copy the pre-existing defaults to the 'sdkconfig' file.

```
cp sdkconfig.defaults.esp32s3 sdkconfig
```

##### build&flash

Build the project and flash it to the board, then run the monitor tool to view the output via serial port:

```
idf.py build

idf.py flash monitor 
```

To enter monitor only mode and also reset the ESP32-S3 you can use the following:
```
idf.py monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

### Modify speech commands

We recommend using MultiNet6 or newer models.   
Here's a simple example to modify speech commands in the code.  
You can also modify the default command list, please refer to [document](https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/speech_command_recognition/README.html) for more details.

```
// MultiNet6
    // Note: Please create multinet handle before adding speech commands

    esp_mn_commands_clear();                       // Clear commands that already exist 
    esp_mn_commands_add(1, "turn on the light");   // add a command
    esp_mn_commands_add(2, "turn off the light");  // add a command
    esp_mn_commands_update();                      // update commands
    multinet->print_active_speech_commands(model_data);     // print active commands
```


### Modifying Wake Word

There will be changes done to the esp-sr framework which is what skainet uses for the wake words and command recognition. Updating to the latest framework version can provide additional wake words when needed.

[ESP-SR Framework](https://github.com/espressif/esp-sr)

