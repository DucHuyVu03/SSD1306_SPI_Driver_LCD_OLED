// 0:32-05/06/24
#include <linux/spi/spi.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/types.h>

#define N_SPI_MINORS 32  // Adjust as needed
#define SSD1306_MAJOR 272 // Use an available major number

//Define ioctl conditions
#define SSD1306_IOC_MAGIC 'k'
#define SSD1306_IOC_SET_CONTRAST _IOW(SSD1306_IOC_MAGIC, 1, __u8)
#define SSD1306_IOC_SET_DISPLAY_ON _IO(SSD1306_IOC_MAGIC, 2)
#define SSD1306_IOC_SET_DISPLAY_OFF _IO(SSD1306_IOC_MAGIC, 3)
#define SSD1306_IOC_ENTIRE_DISPLAY_ON _IO(SSD1306_IOC_MAGIC, 4)
#define SSD1306_IOC_ENTIRE_DISPLAY_RESUME _IO(SSD1306_IOC_MAGIC, 5) //Turn off all LEDs that are not stored in RAM 
#define SSD1306_IOC_BLINK_ALL_LED_TEST _IO(SSD1306_IOC_MAGIC, 6) //Turn ON and OFF all LEDs 5 times for display function testing 
#define SSD1306_IOC_DISPLAY_INIT _IO(SSD1306_IOC_MAGIC, 7) //Initiate Display with recommended setting 

//Other marcos
#define SSD1306_IOC_MAXNR 1
#define SSD1306_HEIGHT 128
#define SSD1306_WIDTH 64

//Define physical pins
#define SSD1306_DC 23


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
    struct mutex spi_lock;  
    uint32_t speed_hz;
    uint32_t mode;
};

//Ham open
static int ssd1306_open(struct inode *inode, struct file *filp);
//Ham release
static int ssd1306_release(struct inode *inode, struct file *filp); 
//Ham write
static ssize_t ssd1306_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos); 
//Ham ioctl
static long ssd1306_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
//Ham send command
static int ssd1306_send_command(struct ssd1306_data *ssd1306, u8 command);
//Ham send data
static int ssd1306_send_data(struct ssd1306_data *ssd1306, const u8 *data, size_t len);

