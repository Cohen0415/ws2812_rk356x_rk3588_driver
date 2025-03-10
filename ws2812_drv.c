/*
*
*   file: ws2812_drv.c
*   date: 2024-09-03
*   notice:
*		The ws2812 driver is suitable for RK356x and RK3588.
*   	The ws2812 driver can only control a signle RGB LED at a time.
*
*	usage:
*		1、Before compilation, you need to enable the corresponding macro definition based on the target chip.
*
*		2、APP example:

		#include <stdio.h>
		#include <string.h>
		#include <sys/types.h>
		#include <sys/stat.h>
		#include <fcntl.h>
		#include <unistd.h>
		#include <stdlib.h>

		struct ws2812_mes {
			unsigned int gpiochip;      // GPIO chip number for the data pin
			unsigned int gpionum;       // GPIO number for the data pin
			unsigned int lednum;        // The ordinal number of the LED on the strip to be controlled, starting from 1
			unsigned char color[3];     // color[0]:color[1]:color[2] R:G:B      
		};

		int main(int argc, char **argv)
		{
			struct ws2812_mes ws2812;
			int fd;

			ws2812.gpiochip = 3;       
			ws2812.gpionum  = 26;
			ws2812.lednum   = 1;
			ws2812.color[0] = 0xff;
			ws2812.color[1] = 0x00;
			ws2812.color[2] = 0x00;

			fd = open("/dev/ws2812", O_RDWR);

			write(fd, &ws2812, sizeof(struct ws2812_mes));

			close(fd);

			return 0;
		}
*
*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/delay.h>

#define DEV_NAME            			"ws2812"
#define LED_NUM_MAX						(20)
//#define DEBUG

// Enable the corresponding macro definition based on the target chip
#define RK356x
//#define RK3588

#ifdef RK356x
/* RK356x GPIO BASE */
#define GPIO0_BASE_ADDR					UL(0xFDD60000)
#define GPIO1_BASE_ADDR					UL(0xFE740000)
#define GPIO2_BASE_ADDR					UL(0xFE750000)
#define GPIO3_BASE_ADDR					UL(0xFE760000)
#define GPIO4_BASE_ADDR					UL(0xFE770000)
#endif
#ifdef RK3588
/* RK3588 GPIO BASE */
#define GPIO0_BASE_ADDR					UL(0xFD8A0000)
#define GPIO1_BASE_ADDR					UL(0xFEC20000)
#define GPIO2_BASE_ADDR					UL(0xFEC30000)
#define GPIO3_BASE_ADDR					UL(0xFEC40000)
#define GPIO4_BASE_ADDR					UL(0xFEC50000)
#endif

/* GPIO Level REG OFFSET */
#define GPIO_SWPORT_DR_L_OFFSET			(0x0000)
#define GPIO_SWPORT_DR_H_OFFSET			(0x0004)
/* GPIO Direction REG OFFSET */
#define GPIO_SWPORT_DDR_L_OFFSET		(0x0008)
#define GPIO_SWPORT_DDR_H_OFFSET		(0x000C)

static volatile unsigned int *GPIO_DIR_REG;
static volatile unsigned int *GPIO_LEVEL_REG;

static struct ws2812_mes {
    unsigned int gpiochip;      		// GPIO chip number for the data pin
    unsigned int gpionum;       		// GPIO number for the data pin
    unsigned int lednum;        		// The ordinal number of the LED on the strip to be controlled, starting from 1
    unsigned char color[3];     		// color[0]:color[1]:color[2] R:G:B     
}ws2812;

static int major = 0;
static struct class *ws2812_class;
static int bit;
static unsigned int temp;

static int ws2812_drv_open(struct inode *node, struct file *file)
{
	return 0;
}

static void ws2812_reset(void)
{
	/* Reset: Pull low for > 280us */
	temp &= (~(1 << bit));
	*GPIO_LEVEL_REG = temp;
	udelay(300);
}

static void ws2812_write_frame_0(void)
{
	/* T0H: Pull high for 220ns ~ 380ns */
	temp |= 1 << bit;
	*GPIO_LEVEL_REG = temp;		/* Execute once, approx. XXns */
	*GPIO_LEVEL_REG = temp;
	*GPIO_LEVEL_REG = temp;		

	/* T0L: Pull low for 580ns ~ 1.6us */
	temp &= (~(1 << bit));
	*GPIO_LEVEL_REG = temp;		/* Execute once, approx. XXns */
	*GPIO_LEVEL_REG = temp;
	*GPIO_LEVEL_REG = temp;
	*GPIO_LEVEL_REG = temp;
	*GPIO_LEVEL_REG = temp;
	*GPIO_LEVEL_REG = temp;
	*GPIO_LEVEL_REG = temp;
	*GPIO_LEVEL_REG = temp;
}

static void ws2812_write_frame_1(void)
{
	/* T1H: Pull high for 580ns ~ 1us */
	temp |= 1 << bit;
	*GPIO_LEVEL_REG = temp;		/* Execute once, approx. XXns */
	*GPIO_LEVEL_REG = temp;
	*GPIO_LEVEL_REG = temp;
	*GPIO_LEVEL_REG = temp;
	*GPIO_LEVEL_REG = temp;
	*GPIO_LEVEL_REG = temp;
	*GPIO_LEVEL_REG = temp;
	*GPIO_LEVEL_REG = temp;

	/* T1L: Pull low for 220ns ~ 420ns */		
	temp &= (~(1 << bit));
	*GPIO_LEVEL_REG = temp;		/* Execute once, approx. xxns */
	*GPIO_LEVEL_REG = temp;
	*GPIO_LEVEL_REG = temp;
}

static void ws2812_write_byte(unsigned char byte)
{
	int i = 0;

	for(i = 0; i < 8; i++)
	{
		if((byte << i) & 0x80)
			ws2812_write_frame_1();
	  	else
			ws2812_write_frame_0();
	}
}

static ssize_t ws2812_drv_write(struct file *filp, const char __user * buf, size_t count, loff_t * ppos)
{
	int err;
	struct ws2812_mes ws2812_usr;
	int step;
	int i = 1;

	err = copy_from_user(&ws2812_usr, buf, sizeof(ws2812_usr));
	if(err != 0)
	{
		printk(KERN_ERR"get ws2812 struct err!\n");
		return err;
	}

#ifdef DEBUG
	printk("ws2812_usr.gpiochip : %d\n", ws2812_usr.gpiochip);
	printk("ws2812_usr.gpionum : %d\n", ws2812_usr.gpionum);
	printk("ws2812_usr.lednum : %d\n", ws2812_usr.lednum);
	printk("ws2812_usr.color[0] : %d\n", ws2812_usr.color[0]);
	printk("ws2812_usr.color[1] : %d\n", ws2812_usr.color[1]);
	printk("ws2812_usr.color[2] : %d\n", ws2812_usr.color[2]);
#endif

	if(ws2812_usr.gpiochip < 0 || ws2812.gpiochip > 4)
	{
		printk(KERN_ERR"ws2812.gpiochip must >= 0 && <= 4\n");
		return -1;
	}

	if(ws2812_usr.gpionum < 0 || ws2812_usr.gpionum > 31)
	{
		printk(KERN_ERR"ws2812.gpionum must >= 0 && <= 31\n");
		return -1;
	}

	if(ws2812_usr.lednum < 1 || ws2812_usr.lednum > LED_NUM_MAX)
	{
		printk(KERN_ERR"ws2812.lednum must >= 1 && <= %d\n", LED_NUM_MAX);
		return -1;
	}
	
	/* step: 0-15 uses GPIO_SWPORT_DR_L_OFFSET, 16-31 uses GPIO_SWPORT_DR_H_OFFSET */
	step = ws2812_usr.gpionum / 15;
	if(ws2812_usr.gpiochip == 0)
	{
		/* 1. GPIO Multiplexing */
		/* In the relevant multiplexing registers, they are all configured as GPIO mode by default, so the GPIO multiplexing configuration is omitted here */
		/* 2、GPIO Mode */
		GPIO_DIR_REG	= ioremap(GPIO0_BASE_ADDR + GPIO_SWPORT_DDR_L_OFFSET+(step*(0x4)), 4);
		/* 3、GPIO Level */
		GPIO_LEVEL_REG 	= ioremap(GPIO0_BASE_ADDR + GPIO_SWPORT_DR_L_OFFSET+(step*(0x4)), 4);
	}
	else
	{
		/* 1. GPIO Multiplexing */
		/* In the relevant multiplexing registers, they are all configured as GPIO mode by default, so the GPIO multiplexing configuration is omitted here */
		/* 2、GPIO Mode */
		GPIO_DIR_REG	= ioremap(GPIO1_BASE_ADDR + (0x10000*(ws2812_usr.gpiochip-1)) + GPIO_SWPORT_DDR_L_OFFSET+(step*(0x4)), 4);
		/* 3、GPIO Level */
		GPIO_LEVEL_REG	= ioremap(GPIO1_BASE_ADDR + (0x10000*(ws2812_usr.gpiochip-1)) + GPIO_SWPORT_DR_L_OFFSET+(step*(0x4)), 4);

#ifdef DEBUG
		printk("GPIO_DIR_REG : 0x%lX\n", GPIO1_BASE_ADDR + (0x10000*(ws2812_usr.gpiochip-1)) + GPIO_SWPORT_DDR_L_OFFSET+(step*(0x4)));
		printk("GPIO_LEVEL_REG : 0x%lX\n", GPIO1_BASE_ADDR + (0x10000*(ws2812_usr.gpiochip-1)) + GPIO_SWPORT_DR_L_OFFSET+(step*(0x4)));
#endif
	}
	
	if(GPIO_LEVEL_REG == NULL || GPIO_DIR_REG == NULL)
	{
		printk(KERN_ERR"GPIO_LEVEL_REG or GPIO_DIR_REG is NULL\n");
		return -1;
	}

	bit = ws2812_usr.gpionum % 16;

	/* Set GPIO Multiplexing */
	/* In the relevant multiplexing registers, they are all configured as GPIO mode by default, so the GPIO multiplexing configuration is omitted here */

	/* Set GPIO Mode to Output */
	temp = *GPIO_DIR_REG;
	temp |= 1 << (16 + bit);
	temp |= 1 << bit;
	*GPIO_DIR_REG = temp;

	/* Set initial GPIO level to high */
	temp = *GPIO_LEVEL_REG;
	temp |= 1 << (16 + bit);
	temp |= 1 << bit;
	*GPIO_LEVEL_REG = temp;

	ws2812_reset();
	
	for(i = 1; i < ws2812_usr.lednum; i++)
	{
		ws2812_write_byte(0x00);		
		ws2812_write_byte(0x00);		
		ws2812_write_byte(0x00);		
	}
	ws2812_write_byte(ws2812_usr.color[1]);		// color G
	ws2812_write_byte(ws2812_usr.color[0]);		// color R
	ws2812_write_byte(ws2812_usr.color[2]);		// color B

	ws2812_reset();

#ifdef DEBUG
	printk("ws2812 write over!\n");
#endif

	return 0;
}

static int ws2812_drv_close(struct inode *node, struct file *file)
{	
	iounmap(GPIO_LEVEL_REG);
	iounmap(GPIO_DIR_REG);

	GPIO_LEVEL_REG = NULL;
	GPIO_DIR_REG = NULL;

	return 0;
}

static struct file_operations ws2812_fops = {
	.owner = THIS_MODULE,
	.open = ws2812_drv_open,
	.release = ws2812_drv_close,
	.write = ws2812_drv_write,
};

static __init int ws2812_init(void)
{
	printk(KERN_INFO"Load the ws2812 module successfully!\n");

	major = register_chrdev(0, "rk_ws2812", &ws2812_fops);  

	ws2812_class = class_create(THIS_MODULE, "rk_ws2812_class");
	if (IS_ERR(ws2812_class)) {
		printk(KERN_ERR"%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
		unregister_chrdev(major, "rk_ws2812");
		return PTR_ERR(ws2812_class);
	}

	device_create(ws2812_class, NULL, MKDEV(major, 0), NULL, "ws2812");

	return 0;
}
module_init(ws2812_init);

static __exit void ws2812_exit(void)
{
	printk(KERN_INFO"the ws2812 module has been remove!\n");

	device_destroy(ws2812_class, MKDEV(major, 0));
	class_destroy(ws2812_class);
	unregister_chrdev(major, "rk_ws2812");

	if(GPIO_LEVEL_REG != NULL)
		iounmap(GPIO_LEVEL_REG);
	if(GPIO_DIR_REG != NULL)
		iounmap(GPIO_DIR_REG);
	GPIO_LEVEL_REG = NULL;
	GPIO_DIR_REG = NULL;
}
module_exit(ws2812_exit);

MODULE_AUTHOR("Cohen0415");
MODULE_LICENSE("GPL");
