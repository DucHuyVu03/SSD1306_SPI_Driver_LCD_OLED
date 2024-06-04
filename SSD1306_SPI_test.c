// 0:32-05/06/24
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>

#define DEVICE_PATH "/dev/ssd1306"

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

//Define physical pins
#define SSD1306_DC 23

int main()
{
    int fd;
    int data;
    //Open device
    fd = open(DEVICE_PATH, O_WRONLY);
    if (fd<0)
    {
        perror("Failed to open device");
        printf("Failed to open device");
        return errno;
    }

    ioctl(fd, SSD1306_IOC_DISPLAY_INIT);

    if (ioctl(fd, SSD1306_IOC_SET_CONTRAST, 255)<0)
    {
        perror("Failed to open set brightness");
        close(fd);
        return errno;
    }
    while(1)
    {
        ioctl(fd, SSD1306_IOC_SET_DISPLAY_ON);
        ioctl(fd, SSD1306_IOC_BLINK_ALL_LED_TEST);
        ioctl(fd,SSD1306_IOC_SET_DISPLAY_OFF);
        usleep(10000);
    }
    close(fd);
    return 0;
}