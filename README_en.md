**Language:**
- [中文](README.md)
- [English](README_en.md)

# WS2812 Driver
1. This is a WS2812 driver for RK356x and RK3588 platforms. The driver controls the LED by manipulating the timing of signal level changes to achieve the desired delays.
2. This control method has a limitation: if the control frequency is too high, the LED may flicker erratically.
3. You can refer to the SPI control method for an alternative approach: https://github.com/Cohen0415/ws2812_spi_app.git

# Usage
## Get the Source Code
```shell
git clone git@github.com:Cohen0415/ws2812_rk356x_rk3588_driver.git
```

## Compile the Driver
Open the provided Makefile, modify the kernel path and cross-compiler toolchain, and run make to compile:
```shell
make
```

## Compile the Application
If you are using a Buildroot system, you need to use a cross-compiler toolchain to compile the application. For my board running Ubuntu:
```shell
sudo gcc -o ws2812_app ws2812_app.c
```

## Run the Application
```shell
# Turn on the first LED in red
sudo ./ws2812_app 1 ff0000

# Turn on the second LED in blue
sudo ./ws2812_app 2 0000ff
```
