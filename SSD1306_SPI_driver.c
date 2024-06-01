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

#define N_SPI_MINORS 32  // Adjust as needed
#define SPIDEV_MAJOR 271 // Use an available major number

static DECLARE_BITMAP(minors, N_SPI_MINORS);
static struct class *spidev_class;

struct ssd1306_data 
{
    dev_t devt;
    struct spi_device *spi;
    struct list_head device_entry;
    struct cdev cdev;
    unsigned users;
    uint8_t *tx_buffer;
    uint8_t *rx_buffer;
    struct mutex buf_lock;
    uint32_t speed_hz;
};

static LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_lock);

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

static int ssd1306_release(struct inode *inode, struct file *filp) 
{
    struct ssd1306_data *ssd1306 = filp->private_data;

    mutex_lock(&device_list_lock);
    ssd1306->users--;
    mutex_unlock(&device_list_lock);

    return 0;
}

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

static const struct file_operations ssd1306_fops = {
    .owner = THIS_MODULE,
    .open = ssd1306_open,
    .release = ssd1306_release,
    .write = ssd1306_write,
    .read = ssd1306_read,
    .llseek = no_llseek,
};

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

    minor = find_first_zero_bit(minors, N_SPI_MINORS);
    if (minor < N_SPI_MINORS) {
        set_bit(minor, minors);
        ssd1306->devt = MKDEV(SPIDEV_MAJOR, minor);
        dev = device_create(spidev_class, &spi->dev, ssd1306->devt, ssd1306, "spidev%d.%d", spi->master->bus_num, spi->chip_select);
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

static int ssd1306_remove(struct spi_device *spi) 
{
    struct ssd1306_data *ssd1306 = spi_get_drvdata(spi);

    device_destroy(spidev_class, ssd1306->devt);
    clear_bit(MINOR(ssd1306->devt), minors);

    list_del(&ssd1306->device_entry);
    kfree(ssd1306);

    return 0;
}

static struct spi_driver ssd1306_driver = {
    .driver = {
        .name = "ssd1306",
        .owner = THIS_MODULE,
    },
    .probe = ssd1306_probe,
    .remove = ssd1306_remove,
};

static int __init ssd1306_init(void) 
{
    int status;

    spidev_class = class_create(THIS_MODULE, "spidev");
    if (IS_ERR(spidev_class)) {
        return PTR_ERR(spidev_class);
    }

    status = register_chrdev(SPIDEV_MAJOR, "spidev", &ssd1306_fops);
    if (status < 0) {
        class_destroy(spidev_class);
        return status;
    }

    status = spi_register_driver(&ssd1306_driver);
    if (status < 0) {
        unregister_chrdev(SPIDEV_MAJOR, ssd1306_driver.driver.name);
        class_destroy(spidev_class);
    }

    return status;
}

static void __exit ssd1306_exit(void) 
{
    spi_unregister_driver(&ssd1306_driver);
    unregister_chrdev(SPIDEV_MAJOR, ssd1306_driver.driver.name);
    class_destroy(spidev_class);
}

module_init(ssd1306_init);
module_exit(ssd1306_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("SSD1306 SPI Driver");
MODULE_VERSION("1.0");