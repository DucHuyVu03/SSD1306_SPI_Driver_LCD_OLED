// 0:32-05/06/24
#include <linux/init.h>
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/types.h>
#include <linux/mod_devicetable.h>

#include "SSD1306_SPI_driver.h"

static DECLARE_BITMAP(minors, N_SPI_MINORS);
static struct class *ssd1306_class;

static LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_lock);

//Ham open
static int ssd1306_open(struct inode *inode, struct file *filp) 
{
    struct ssd1306_data *ssd1306;
    int status = -ENXIO;

    mutex_lock(&device_list_lock);

    list_for_each_entry(ssd1306, &device_list, device_entry) 
    {
        if (ssd1306->devt == inode->i_rdev) 
        {
            ssd1306->users++;
            filp->private_data = ssd1306;
            nonseekable_open(inode, filp);
            status = 0;
            break;
        }
    }
    mutex_unlock(&device_list_lock);
    return status;
}

//Ham release
static int ssd1306_release(struct inode *inode, struct file *filp) 
{
    struct ssd1306_data *ssd1306 = filp->private_data;

    mutex_lock(&device_list_lock);
    ssd1306->users--;
    mutex_unlock(&device_list_lock);

    return 0;
}

//Ham write
static ssize_t ssd1306_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos) 
{
    struct ssd1306_data *ssd1306 = filp->private_data;
    ssize_t status = 0;

    if (count > 4096)
        return -ENOMEM;

    if (copy_from_user(ssd1306->tx_buffer, buf, count))
        return -EFAULT;

    mutex_lock(&ssd1306->buf_lock);
    status = spi_write(ssd1306->spi, ssd1306->tx_buffer, count);
    mutex_unlock(&ssd1306->buf_lock);

    return status;
}

//Ham ioctl
static long ssd1306_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct ssd1306_data *ssd1306;
    int status = 0;

	/* Check type and command number */
	if (_IOC_TYPE(cmd) != SSD1306_IOC_MAGIC)
		return -ENOTTY;
    ssd1306 = filp->private_data;

    if (_IOC_NR(cmd) > SSD1306_IOC_MAXNR) return -ENOTTY;

    switch (cmd) {
        case SSD1306_IOC_SET_CONTRAST:
            if (arg > 0xFF) return -EINVAL;
            mutex_lock(&ssd1306->buf_lock);
            ssd1306_send_command(ssd1306, 0x81); // Set contrast command
            ssd1306_send_command(ssd1306, (__u8)arg);
            mutex_unlock(&ssd1306->buf_lock);
            break;
//Turn ON Display
        case SSD1306_IOC_SET_DISPLAY_ON:
            status = ssd1306_send_command(ssd1306, 0xAF);
            break;
//Turn OFF Display
        case SSD1306_IOC_SET_DISPLAY_OFF:
            status = ssd1306_send_command(ssd1306, 0xAE);
            break;
//ALL LED ON
        case SSD1306_IOC_ENTIRE_DISPLAY_ON:
            status = ssd1306_send_command(ssd1306, 0xA5);
            break;
        case SSD1306_IOC_ENTIRE_DISPLAY_RESUME:
            status = ssd1306_send_command(ssd1306, 0xA4);
            break;
//LEDs testing by blinking all LEDs 5 times
        case SSD1306_IOC_BLINK_ALL_LED_TEST:
            for(int i = 0; i<5; i++)
            {
                status = ssd1306_send_command(ssd1306, 0xA5);
                msleep(500);
                status = ssd1306_send_command(ssd1306, 0xA4);
                msleep(500);
            }
            break;
        case SSD1306_IOC_DISPLAY_INIT:
            ssd1306_recommend_init(ssd1306);
            break;
        default:
            return -ENOTTY;
    }
    return status;
}

//Cau truc file operation
static const struct file_operations ssd1306_fops = {
    .owner = THIS_MODULE,
    .open = ssd1306_open,
    .release = ssd1306_release,
    .write = ssd1306_write,
    .unlocked_ioctl = ssd1306_ioctl,
    .llseek = no_llseek,
};

//Ham probe
static int ssd1306_probe(struct spi_device *spi) 
{
    struct ssd1306_data *ssd1306;
    int status;
    unsigned long minor;
    struct device *dev;

    ssd1306 = kzalloc(sizeof(*ssd1306), GFP_KERNEL);
    if (!ssd1306)
        return -ENOMEM;

    ssd1306->spi = spi;
    mutex_init(&ssd1306->buf_lock);
    INIT_LIST_HEAD(&ssd1306->device_entry);

    // Initialize the D/C GPIO pin
    ssd1306->mode = SSD1306_DC;
    status = devm_gpio_request_one(&spi->dev, ssd1306->mode, GPIOF_OUT_INIT_LOW, "ssd1306_dc");
    if (status) {
        dev_err(&spi->dev, "Failed to request GPIO for D/C pin\n");
        kfree(ssd1306);
        return status;
    }

    minor = find_first_zero_bit(minors, N_SPI_MINORS);
    if (minor < N_SPI_MINORS) {
        set_bit(minor, minors);
        ssd1306->devt = MKDEV(SSD1306_MAJOR, minor);
        dev = device_create(ssd1306_class, &spi->dev, ssd1306->devt, ssd1306, "SSD1306_SPI_driver%d.%d", spi->master->bus_num, spi->chip_select);
        status = IS_ERR(dev) ? PTR_ERR(dev) : 0;
    } else {
        dev_dbg(&spi->dev, "no minor number available!\n");
        status = -ENODEV;
    }

    if (status == 0) {
        list_add(&ssd1306->device_entry, &device_list);
        spi_set_drvdata(spi, ssd1306);
    } else {
        kfree(ssd1306);
    }

    return status;
}

//Ham remove
static void ssd1306_remove(struct spi_device *spi)
{
	struct ssd1306_data	*ssd1306 = spi_get_drvdata(spi);

	/* prevent new opens */
	mutex_lock(&device_list_lock);
	/* make sure ops on existing fds can abort cleanly */
	mutex_lock(&ssd1306->spi_lock);
	ssd1306->spi = NULL;
	mutex_unlock(&ssd1306->spi_lock);

	list_del(&ssd1306->device_entry);
	device_destroy(ssd1306_class, ssd1306->devt);
	clear_bit(MINOR(ssd1306->devt), minors);
	if (ssd1306->users == 0)
		kfree(ssd1306);
    // Free the GPIO pin
    

	mutex_unlock(&device_list_lock);
}
//Khai bao id
static const struct spi_device_id ssd1306_id[] = {
    { "ssd1306", 0 },
    { }
};
MODULE_DEVICE_TABLE(spi, ssd1306_id);
//Khai bao driver
static struct spi_driver ssd1306_driver = {
    .driver = {
        .name = "ssd1306",
        .owner = THIS_MODULE,
    },
    .probe = ssd1306_probe,
    .remove = ssd1306_remove,
    .id_table = ssd1306_id,
};

//Ham send command
static int ssd1306_send_command(struct ssd1306_data *ssd1306, u8 command) {
    int ret;
    struct spi_message msg;
    struct spi_transfer xfer = {
        .tx_buf = &command,
        .len = 1,
        .cs_change = 0,
    };

    mutex_lock(&ssd1306->spi_lock);
    if (!ssd1306->spi) {
        mutex_unlock(&ssd1306->spi_lock);
        return -ENODEV;
    }

    // Set the D/C line to low (0) for command
    gpio_set_value(ssd1306->mode, 0);

    spi_message_init(&msg);
    spi_message_add_tail(&xfer, &msg);
    ret = spi_sync(ssd1306->spi, &msg);
    mutex_unlock(&ssd1306->spi_lock);

    return ret;
}

//Ham send data
static int ssd1306_send_data(struct ssd1306_data *ssd1306, const u8 *data, size_t len) {
    int ret;
    struct spi_message msg;
    struct spi_transfer xfer = {
        .tx_buf = data,
        .len = len,
        .cs_change = 0,
    };

    mutex_lock(&ssd1306->spi_lock);
    if (!ssd1306->spi) {
        mutex_unlock(&ssd1306->spi_lock);
        return -ENODEV;
    }

    // Set the D/C line to high for data
    gpio_set_value(ssd1306->mode, 1);

    spi_message_init(&msg);
    spi_message_add_tail(&xfer, &msg);
    ret = spi_sync(ssd1306->spi, &msg);
    mutex_unlock(&ssd1306->spi_lock);

    return ret;
}
static void ssd1306_recommend_init(struct ssd1306_data *ssd1306)
{
    ssd1306_send_command(ssd1306, 0xAE); //Display OFF
    ssd1306_send_command(ssd1306, 0xD5); //Set Display Clock Divide Ratio and Oscillator Frequency
    ssd1306_send_command(ssd1306, 0x80); //Default setting for Display Clock Divide Ratio and Oscillator Frequency
    ssd1306_send_command(ssd1306, 0xA8); //Set multiplex ratio
    ssd1306_send_command(ssd1306, 0x3F); //64 COM lines
    ssd1306_send_command(ssd1306, 0xD3); //Set Display offset
    ssd1306_send_command(ssd1306, 0x00); //No offset
    ssd1306_send_command(ssd1306, 0x40); //Set first line as start line
    ssd1306_send_command(ssd1306, 0x8D); //Set charge bump 
    ssd1306_send_command(ssd1306, 0x14); //Enable charge bump during display on
    ssd1306_send_command(ssd1306, 0x20); //Set memory address mode
    ssd1306_send_command(ssd1306, 0x00); //Set Horizontal Address mode
    ssd1306_send_command(ssd1306, 0xA0); //Set segment remap with column address start from left to right 
    ssd1306_send_command(ssd1306, 0xC0); //Set com output scan direction, scan from left to right 
    ssd1306_send_command(ssd1306, 0xDA); //Set com pins hardware configuration
    ssd1306_send_command(ssd1306, 0x12); //Alternative com pin configuration, disable com left/right remap
    ssd1306_send_command(ssd1306, 0x81); //Set contrast control
    ssd1306_send_command(ssd1306, 0x80); //Set Contrast to 128
    ssd1306_send_command(ssd1306, 0xD9); //Set pre-charge period
    ssd1306_send_command(ssd1306, 0xF1); //Phase 1 period of 15 DCLK, Phase 2 period of 1 DCLK
    ssd1306_send_command(ssd1306, 0xDB); //Set Vcomh deselect level
    ssd1306_send_command(ssd1306, 0x20); //Vcomh deselect level ~ 0.77 Vcc
    ssd1306_send_command(ssd1306, 0xA4); //Entire display ON, resume to RAM content display
    ssd1306_send_command(ssd1306, 0xA6); //Set Display in Normal Mode, 1 = ON, 0 = OFF
    ssd1306_send_command(ssd1306, 0x2E); //Deactivate scroll
    ssd1306_send_command(ssd1306, 0xAF); //Display ON in normal mode
}

// static void ssd1306_set_pixel(struct ssd1306_data *ssd1306,int x, int y)
// {
//     if(x < 0 || x >= SSD1306_WIDTH || y < 0 || y >= SSD1306_HEIGHT)
//     {
//         return ENOMEM; 
//     }
//     else
//     {
//         int page = y / 8;
//         int 
//     }
// }

// ham init
static int __init ssd1306_init(void) 
{
    int status;
    printk(KERN_INFO "Initiate SSD1306 SPI Driver. Credit: H_D_T\n");
    ssd1306_class = class_create(THIS_MODULE, "SSD1306_SPI_driver");
    if (IS_ERR(ssd1306_class)) {
        return PTR_ERR(ssd1306_class);
    }

    status = register_chrdev(SSD1306_MAJOR, "SSD1306_SPI_driver", &ssd1306_fops);
    if (status < 0) {
        class_destroy(ssd1306_class);
        return status;
    }

    status = spi_register_driver(&ssd1306_driver);
    if (status < 0) {
        unregister_chrdev(SSD1306_MAJOR, ssd1306_driver.driver.name);
        class_destroy(ssd1306_class);
    }

    return status;
}
//ham exit
static void __exit ssd1306_exit(void) 
{
    spi_unregister_driver(&ssd1306_driver);
    unregister_chrdev(SSD1306_MAJOR, ssd1306_driver.driver.name);
    class_destroy(ssd1306_class);
}

module_init(ssd1306_init);
module_exit(ssd1306_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("HUY_DUY_TIEN");
MODULE_DESCRIPTION("SSD1306 SPI Driver");
MODULE_VERSION("1.0");

