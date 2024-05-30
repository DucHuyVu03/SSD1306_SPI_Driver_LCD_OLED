#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <SSD1306_SPI.h>

#define SPIDEV_MAJOR			271  //12 bit(0-4095). Number of the device in the OS
#define SPI_MINORS_NUM			32	 //Number of devices

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