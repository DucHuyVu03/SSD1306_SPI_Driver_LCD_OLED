//??
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/spi/spi.h>
#include <linux/gpio.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#define SPIDEV_MAJOR			271  //12 bit(0-4095). Number of the device in the OS
#define SPI_MINORS_NUM			32	 //Number of devices

struct SSD1306_SPI_DATA
{
    struct spi_device *ssd1306_spi ;
    unsigned user;
    u8 *tx_buffer;
    u8 *rx_buffer;
    u32 speed_hz;                     
};


//Ham init
static int __init SSD1306_SPI_init(void)
{
    int status;
    printk(KERN_INFO "SSD1306 SPI Driver Initiate\n");
    //Driver Register
    status = register_chrdev(SPIDEV_MAJOR, "spi", &spidev_fops);
    if(status <0) 
        return status;
    //Driver class register
	status = class_register(&spidev_class);
	if (status) {
		unregister_chrdev(SPIDEV_MAJOR, spidev_spi_driver.driver.name);
		return status;
	}
}
//Ham exit
static void __exit SSD1306_SPI_exit(void)
{
    printk(KERN_INFO "SSD1306 SPI Driver Exit\n");
    spi_unregister_driver(&spidev_spi_driver);
	class_unregister(&spidev_class);
	unregister_chrdev(SPIDEV_MAJOR, spidev_spi_driver.driver.name);
}
//Ham open
static int SSD1306_SPI_open(struct inode *inode, struct file *filp)
{
    struct SSD1306_SPI_DATA *SSD1306_device = NULL, *iter;
    int status = -ENXIO;

    mutex_lock(&device_list_lock);

    list_for_each_entry(iter, &device_list, device_entry) {
        if (iter->devt == inode->i_rdev) {
            status = 0;
            SSD1306_device = iter;
            break;
        }
    }

    if (!SSD1306_device) {
        pr_debug("ssd1306: nothing for minor %d\n", iminor(inode));
        goto err_find_dev;
    }

    if (!SSD1306_device->tx_buffer) {
        SSD1306_device->tx_buffer = kmalloc(4096, GFP_KERNEL);  // Adjust size as needed
        if (!SSD1306_device->tx_buffer) {
            status = -ENOMEM;
            goto err_find_dev;
        }
    }

    if (!SSD1306_device->rx_buffer) {
        SSD1306_device->rx_buffer = kmalloc(4096, GFP_KERNEL);  // Adjust size as needed
        if (!SSD1306_device->rx_buffer) {
            status = -ENOMEM;
            goto err_alloc_rx_buf;
        }
    }

    SSD1306_device->users++;
    filp->private_data = SSD1306_device;
    stream_open(inode, filp);

    mutex_unlock(&device_list_lock);
    return 0;

err_alloc_rx_buf:
    kfree(SSD1306_device->tx_buffer);
    SSD1306_device->tx_buffer = NULL;
err_find_dev:
    mutex_unlock(&device_list_lock);
    return status;
}

//Ham release
//Ham ioctl 
//Ham read
//Ham write

module_init(SSD1306_SPI_init);
module_exit(SSD1306_SPI_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Huy_Duy_Tien");
MODULE_DESCRIPTION("SSD1306 SPI Driver build by Huy_Duy_Tien");
MODULE_VERSION("0.1");