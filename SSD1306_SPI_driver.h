#include <linux/spi/spi.h>
#include <linux/mutex.h>
#include <linux/cdev.h>

#define N_SPI_MINORS 32  // Adjust as needed
#define SPIDEV_MAJOR 271 // Use an available major number

#define SSD1306_IOC_MAGIC 'k'
#define SSD1306_IOC_SET_CONTRAST _IOW(SSD1306_IOC_MAGIC, 1, __u8)
#define SSD1306_IOC_MAXNR 1

//Define data
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

//Ham open
static int ssd1306_open(struct inode *inode, struct file *filp);
//Ham release
static int ssd1306_release(struct inode *inode, struct file *filp); 
//Ham write
static ssize_t ssd1306_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos); 
//Ham ioctl
static long ssd1306_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);