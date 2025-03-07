**Language:**
- [中文](README.md)
- [English](README_en.md)

# ws2812驱动
1. 这是一个基于rk356x和rk3588的ws2812驱动，该驱动是利用电平翻转的时间来控制延时，从而点亮LED。
2. 此种控制方法存在缺陷，若控制频率过高，会出现LED乱闪。
3. 可以参考SPI控制：https://github.com/Cohen0415/ws2812_spi_app.git

# 使用
## 获取源码
```shell
git clone git@github.com:Cohen0415/ws2812_rk356x_rk3588_driver.git
```

## 编译驱动程序
打开提供的Makefile，修改内核路径和交叉编译工具链，执行make编译：
```shell
make
```

## 编译应用程序
如果你使用的是buildroot系统，需要使用交叉编译工具链编译。我的板卡是Ubuntu系统：
```shell
sudo gcc -o ws2812_app ws2812_app.c
```

## 执行应用程序
```shell
# 控制第一个led亮红色
sudo ./ws2812_app 1 ff0000

# 控制第二个led亮蓝色
sudo ./ws2812_app 2 0000ff
```
