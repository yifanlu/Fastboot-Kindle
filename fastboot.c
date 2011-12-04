/*
 * Copyright (C) 2008 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the 
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED 
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <ctype.h>

#include <sys/time.h>

#include "fastboot.h"

static usb_handle *usb = 0;
static const char *serial = 0;
static unsigned short vendor_id = 0;

void die(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr,"error: ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr,"\n");
    va_end(ap);
    exit(1);
}    

void get_my_path(char *path);

#ifdef _WIN32
void *load_file(const char *fn, unsigned *_sz);
#else
void *load_file(const char *fn, unsigned *_sz)
{
    char *data;
    int sz;
    int fd;

    data = 0;
    fd = open(fn, O_RDONLY);
    if(fd < 0) return 0;

    sz = lseek(fd, 0, SEEK_END);
    if(sz < 0) goto oops;

    if(lseek(fd, 0, SEEK_SET) != 0) goto oops;

    data = (char*) malloc(sz);
    if(data == 0) goto oops;

    if(read(fd, data, sz) != sz) goto oops;
    close(fd);

    if(_sz) *_sz = sz;
    return data;

oops:
    close(fd);
    if(data != 0) free(data);
    return 0;
}
#endif

int match_fastboot(usb_ifc_info *info)
{
    if(!(vendor_id && (info->dev_vendor == vendor_id)) &&
       (info->dev_vendor != 0x1949))    // Lab126
            return -1;
    if(info->ifc_class != 0xff) return -1;
    if(info->ifc_subclass != 0x42) return -1;
    if(info->ifc_protocol != 0x03) return -1;
    // require matching serial number if a serial number is specified
    // at the command line with the -s option.
    if (serial && strcmp(serial, info->serial_number) != 0) return -1;
    return 0;
}

int list_devices_callback(usb_ifc_info *info)
{
    if (match_fastboot(info) == 0) {
        char* serial = info->serial_number;
        if (!info->writable) {
            serial = "no permissions"; // like "adb devices"
        }
        if (!serial[0]) {
            serial = "????????????";
        }
        // output compatible with "adb devices"
        printf("%s\tfastboot\n", serial);
    }

    return -1;
}

usb_handle *open_device(void)
{
    static usb_handle *usb = 0;
    int announce = 1;

    if(usb) return usb;
    
    for(;;) {
        usb = usb_open(match_fastboot);
        if(usb) return usb;
        if(announce) {
            announce = 0;    
            fprintf(stderr,"< waiting for device >\n");
        }
        sleep(1);
    }
}

void list_devices(void) {
    // We don't actually open a USB device here,
    // just getting our callback called so we can
    // list all the connected devices.
    usb_open(list_devices_callback);
}

void usage(void)
{
    fprintf(stderr,
/*           1234567890123456789012345678901234567890123456789012345678901234567890123456 */
            "usage: fastboot [ <option> ] <command>\n"
            "\n"
            "commands:\n"
            "  getvar <variable>                        display a bootloader or idme variable\n"
            "  setvar <variable> <value>                sets an idme variable\n"
            "  download <filename>                      download data to memory for use with \n"
            "                                             future commands\n"
            "  verify <partition> [ <filename> ]        verify downloaded data. required if \n"
            "                                             bootloader is secure\n"
            "  flash <partition> [ <filename> ]         flash downloaded data\n"
            "  eraseall                                 wipe the entire flash memory\n"
            "  erase <partition>                        erase a flash partition\n"
            "  check <partition>                        crc32 hash test the flash memory\n"
            "  boot [ <filename> ]                      boot downloaded data\n"
            "  continue                                 exit fastboot and return to \n"
            "                                             bootloader\n"
            "  reboot                                   reboot the device\n"
            "  powerdown                                shuts down the device\n"
            "  pass                                     sets LED to green\n"
            "  fail                                     sets LED to red\n"
            "\n"
            "variables:\n"
            "  version-bootloader                       (read only) version string for the \n"
            "                                             bootloader\n"
            "  version                                  (read only) version of fastboot \n"
            "                                             protocol supported\n"
            "  product                                  (read only) name of the product\n"
            "  serialno                                 (read only) fastboot serial number\n"
            "  secure                                   (read only) if \"yes\" boot images \n"
            "                                             must be signed\n"
            "  serial                                   (read write) serial number\n"
            "  accel                                    (read write) accelerometer \n"
            "                                             calibration data\n"
            "  mac                                      (read write) MAC address\n"
            "  sec                                      (read write) manufacturing code\n"
            "  pcbsn                                    (read write) PCB serial number\n"
            "  bootmode                                 (read write) diags, fastboot, \n"
            "                                             factory, reset, or main (default)\n"
            "  postmode                                 (read write) slow, factory, or \n"
            "                                             normal (default)\n"
            "\n"
            "partitions:\n"
            "  bootloader                               bootloader, 376KiB\n"
            "  prod                                     overlaps bootloader, 120KiB\n"
            "  bist                                     bist, 256KiB\n"
            "  userdata                                 userdata, 5KiB\n"
            "  userpartition                            userpartition\n"
            "  mbr                                      master boot record\n"
            "  kernel                                   primary kernel\n"
            "  diags_kernel                             diags kernel\n"
            "  system                                   main system (root) partition\n"
            "  diags                                    secondary system (diags) partition\n"
            "  data                                     user data\n"
            "\n"
            "options:\n"
            "  -s <serial number>                       specify device serial number\n"
            "  -i <vendor id>                           specify a custom USB vendor id\n"
        );
    exit(1);
}

static char *strip(char *s)
{
    int n;
    while(*s && isspace(*s)) s++;
    n = strlen(s);
    while(n-- > 0) {
        if(!isspace(s[n])) break;
        s[n] = 0;
    }
    return s;
}

#define MAX_OPTIONS 32
static int setup_requirement_line(char *name)
{
    char *val[MAX_OPTIONS];
    const char **out;
    unsigned n, count;
    char *x;
    int invert = 0;
    
    if (!strncmp(name, "reject ", 7)) {
        name += 7;
        invert = 1;
    } else if (!strncmp(name, "require ", 8)) {
        name += 8;
        invert = 0;
    }

    x = strchr(name, '=');
    if (x == 0) return 0;
    *x = 0;
    val[0] = x + 1;

    for(count = 1; count < MAX_OPTIONS; count++) {
        x = strchr(val[count - 1],'|');
        if (x == 0) break;
        *x = 0;
        val[count] = x + 1;
    }
    
    name = strip(name);
    for(n = 0; n < count; n++) val[n] = strip(val[n]);
    
    name = strip(name);
    if (name == 0) return -1;

        /* work around an unfortunate name mismatch */
    if (!strcmp(name,"board")) name = "product";

    out = malloc(sizeof(char*) * count);
    if (out == 0) return -1;

    for(n = 0; n < count; n++) {
        out[n] = strdup(strip(val[n]));
        if (out[n] == 0) return -1;
    }

    fb_queue_require(name, invert, n, out);
    return 0;
}

static void setup_requirements(char *data, unsigned sz)
{
    char *s;

    s = data;
    while (sz-- > 0) {
        if(*s == '\n') {
            *s++ = 0;
            if (setup_requirement_line(data)) {
                die("out of memory");
            }
            data = s;
        } else {
            s++;
        }
    }
}

#define skip(n) do { argc -= (n); argv += (n); } while (0)
#define require(n) do { if (argc < (n)) usage(); } while (0)

int do_oem_command(int argc, char **argv)
{
    int i;
    char command[256];
    if (argc <= 1) return 0;
    
    command[0] = 0;
    while(1) {
        strcat(command,*argv);
        skip(1);
        if(argc == 0) break;
        strcat(command," ");
    }

    fb_queue_command(command,"");    
    return 0;
}

int main(int argc, char **argv)
{
    int wants_reboot = 0;
    int wants_reboot_bootloader = 0;
    void *data;
    unsigned sz;
    unsigned page_size = 2048;

    skip(1);
    if (argc == 0) {
        usage();
        return 0;
    }

    if (!strcmp(*argv, "devices")) {
        list_devices();
        return 0;
    }

    serial = getenv("KINDLE_SERIAL");

    while (argc > 0) {
        if(!strcmp(*argv, "-s")) {
            require(2);
            serial = argv[1];
            skip(2);
        } else if(!strcmp(*argv, "-i")) {
            char *endptr = NULL;
            unsigned long val;

            require(2);
            val = strtoul(argv[1], &endptr, 0);
            if (!endptr || *endptr != '\0' || (val & ~0xffff))
                die("invalid vendor id '%s'", argv[1]);
            vendor_id = (unsigned short)val;
            skip(2);
        } else if(!strcmp(*argv, "getvar")) {
            require(2);
            fb_queue_display(argv[1], argv[1]);
            skip(2);
		} else if(!strcmp(*argv, "setvar")) {
	        require(3);
	        fb_queue_set(argv[1], argv[2], argv[1]);
	        skip(3);
        } else if(!strcmp(*argv, "download")) {
            char *fname = 0;
            require(2);
            if (argc > 1) {
                fname = argv[1];
                skip(2);
            }
            if (fname == 0) die("cannot determine image filename");
            data = load_file(fname, &sz);
            if (data == 0) die("cannot load '%s'\n", fname);
            fb_queue_download("data", data, sz);
        } else if(!strcmp(*argv, "verify")) {
            char *pname = argv[1];
            char *fname = 0;
			data = 0;
            require(2);
            if (argc > 2) {
                fname = argv[2];
                skip(3);
            } else {
                skip(2);
			}
            if (fname > 0) {
            	data = load_file(fname, &sz);
            	if (data == 0) die("cannot load '%s'\n", fname);
	            fb_queue_download(pname, data, sz);
			}
            fb_queue_verify(pname, sz);
        } else if(!strcmp(*argv, "flash")) {
            char *pname = argv[1];
            char *fname = 0;
			data = 0;
            require(2);
            if (argc > 2) {
                fname = argv[2];
                skip(3);
            } else {
                skip(2);
			}
            if (fname > 0) {
            	data = load_file(fname, &sz);
            	if (data == 0) die("cannot load '%s'\n", fname);
	            fb_queue_download(pname, data, sz);
			}
            fb_queue_flash(pname, sz);
		} else if(!strcmp(*argv, "eraseall")) {	
            fb_queue_command("eraseall", "wiping the flash memory");
	        skip(1);
        } else if(!strcmp(*argv, "erase")) {
            require(2);
            fb_queue_erase(argv[1]);
            skip(2);
        } else if(!strcmp(*argv, "check")) {
            require(2);
            fb_queue_check(argv[1]);
            skip(2);
        } else if(!strcmp(*argv, "boot")) {
            char *pname = argv[1];
            char *fname = 0;
			data = 0;
            require(1);
            if (argc > 1) {
                fname = argv[1];
                skip(2);
            } else {
                skip(1);
			}
            if (fname > 0) {
            	data = load_file(fname, &sz);
            	if (data == 0) die("cannot load '%s'\n", fname);
	            fb_queue_download("boot", data, sz);
			}
	        fb_queue_command("boot", "booting");
        } else if (!strcmp(*argv, "continue")) {
            fb_queue_command("continue", "resuming boot");
            skip(1);
        } else if(!strcmp(*argv, "oem")) {
            argc = do_oem_command(argc, argv);
		} else if(!strcmp(*argv, "reboot")) {
			fb_queue_reboot();
            skip(1);
        } else if (!strcmp(*argv, "powerdown")) {
            fb_queue_command("powerdown", "shutting down");
            skip(1);
        } else if (!strcmp(*argv, "pass")) {
            fb_queue_command("pass", "turning on led");
            skip(1);
        } else if (!strcmp(*argv, "fail")) {
            fb_queue_command("fail", "turning on led");
            skip(1);
        } else {
            usage();
        }
    }

    if (wants_reboot) {
        fb_queue_reboot();
    } else if (wants_reboot_bootloader) {
        fb_queue_command("reboot-bootloader", "rebooting into bootloader");
    }

    usb = open_device();

    fb_execute_queue(usb);
    return 0;
}
