# [Note] This branch has some modifications to make it work on the Generic board, ESP32-S3-Devkit-C and INMP441 mems microphone.
  
## Quick Start with ESP-Skainet for Generic ESP32-S3 DevKit-C 

### Pre-requisite

Now this project runs on IDF version 5.x.

### ESP-Skainet

Make sure you have cloned my project branch with the '--branch'.
Also, need to get all submodules so don't forget the option '--recursive'.

```
git clone --branch ESP32-S3-Devkit-C --recursive git@github.com:0015/esp-skainet.git
```

### Example that can be operated in ESP32-DevKit-C and INMP441 

1. Set your I2s microphone settings 

```
cd esp-skainet/components/hardware_driver/boards/include
```

Open the file, 'esp32_s3_devkit_c.h'
```
#define FUNC_I2S_EN         (1)
#define GPIO_I2S_LRCK       (GPIO_NUM_11)
#define GPIO_I2S_MCLK       (GPIO_NUM_NC)
#define GPIO_I2S_SCLK       (GPIO_NUM_12)
#define GPIO_I2S_SDIN       (GPIO_NUM_10)
#define GPIO_I2S_DOUT       (GPIO_NUM_NC)
``` 

Modify them for your system.


2. Navigate to one example folder 'esp-skainet/examples/en_speech_commands_recognition'.
```
cd ../../../../examples/en_speech_commands_recognition/
```

3. Set Target and Choose your hardware board
```
idf.py set-target esp32s3
idf.py menuconfig
```
Audio hardware board
-> ESP32-S3-DEVKIT-C

4. Build and flash the project.
```
idf.py build
idf.py flash monitor
```

5. Advanced users can add or modify speech commands by using the `idf.py menuconfig` command. 
```
idf.py menuconfig
```