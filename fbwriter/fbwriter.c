
// 
// Copyrights 2022, Varanda Labs Inc.
//
// License: GPL-3
//
// refs:
//     https://learn.watterott.com/hats/rpi-display/fbtft-install/
//     events: https://www.kernel.org/doc/html/v4.15/input/input.html
//             see with either "cat /dev/input/event2 |xxd" or evtest
//             code: https://github.com/Robin329/evtest/blob/master/evtest.c
//

#include <stdio.h>
#include <syslog.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <cairo.h>

#define RED         0b1111100000000000
#define GREEN       0b0000011111100000
#define BLUE        0b0000000000011111

#define FB_DEV_NAME "/dev/fb1"

static int fbfd = -1;
static char *fbp = 0;

static struct fb_var_screeninfo vinfo;
static struct fb_fix_screeninfo finfo;

int fbwriter_open(char * dev_name) 
{
    fbfd = open(dev_name, O_RDWR);
    if (fbfd == -1) {
        syslog(LOG_ERR, "Unable to open secondary display");
        return -1;
    }
    if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo)) {
        syslog(LOG_ERR, "Unable to get secondary display information");
        return -1;
    }
    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo)) {
        syslog(LOG_ERR, "Unable to get secondary display information");
        return -1;
    }

    syslog(LOG_INFO, "framebuffer display is %d x %d %dbps\n", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel);

    fbp = (char*) mmap(0, finfo.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
    if (fbp <= 0) {
        syslog(LOG_ERR, "Unable to create memory mapping");
        close(fbfd);
        return -1;
    }
    return 0;
}

int fbwriter_update(char * rgb565_ptr)
{
    memcpy(fbp, rgb565_ptr,  vinfo.xres * vinfo.yres * (vinfo.bits_per_pixel / 8));
}

void fbwriter_close()
{
    syslog(LOG_INFO, "loop done");
    munmap(fbp, finfo.smem_len);
    close(fbfd);
}

#ifdef FB_TEST
static void fill_pixels(char * buf, uint16_t color, int num_pixels)
{
    // fill color half screen
    uint16_t * ptr = (uint16_t *) buf;
    for (int i=0; i < num_pixels; i++) {
        *ptr++ = color;
    }
}

int main(int argc, char **argv) {
    setlogmask(LOG_UPTO(LOG_DEBUG));
    openlog("fbwriter", LOG_NDELAY | LOG_PID, LOG_USER);
    char * buf = NULL;

    if (argc < 2) {
        printf("File must be given\n");
        return 1;
    }

    cairo_surface_t *image = cairo_image_surface_create_from_png (argv[1]);
 
    cairo_format_t fmt = cairo_image_surface_get_format(image);
    if (fmt != CAIRO_FORMAT_ARGB32) {
        printf("expecting CAIRO_FORMAT_ARGB32 format.\n");
        return 1;
    }

    int width = cairo_image_surface_get_width (image);
    int height = cairo_image_surface_get_height (image);
    printf("width = %d, heigth = %d\n", width, height);

    buf = (char *) malloc(width * height * 2);
    if (!buf) {
        printf("no memo.\n");
        cairo_surface_destroy(image);
        return 1;
    }

    uint32_t * argb32 = (uint32_t *) cairo_image_surface_get_data(image);
    uint16_t * rgb16 = (uint16_t *) buf;
    uint16_t v = 0;
    for (int i = 0; i < width * height; i++) {
        v = (*argb32 &  0b00000000111110000000000000000000) >> 8;
        v |= (*argb32 & 0b00000000000000001111110000000000) >> 5;
        v |= (*argb32 & 0b00000000000000000000000011111000) >> 3;
        *rgb16++ = v;
        argb32++;
    }

    printf("buf = %p\n", buf);

    printf("open fb\n");
    if (fbwriter_open(FB_DEV_NAME)) {
        printf("Could not open fb\n");
        free(buf);
        cairo_surface_destroy(image);
        return 1;
    }
    printf("update fb\n");
#if 1
    fbwriter_update(buf);
#else
    fill_pixels(buf, RED, ((480 * 320) / 2));
    fbwriter_update(buf);
    sleep(2);

    fill_pixels(buf, GREEN, ((480 * 320) / 2));
    fbwriter_update(buf);
    sleep(2);

    fill_pixels(buf, BLUE, ((480 * 320) / 2));
    fbwriter_update(buf);
    sleep(2);
#endif
    fbwriter_close();

    free(buf);
    cairo_surface_destroy(image);
    return 0;
}
#endif
