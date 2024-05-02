/*
lasershark_jack.c - Main host application to talk with Lasershark devices.
Copyright (C) 2012 Jeffrey Nelson <nelsonjm@macpod.net>

This file is part of Lasershark's USB Host App.

Lasershark is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

Lasershark is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Lasershark. If not, see <http://www.gnu.org/licenses/>.
*/

/*
ISOCHRONOUS mode did not work properly with Windows and LaserCube.
The LaserDock library was working correctly using the BULK transfer mode.
This code replaces the USB writing part with the LaserDock library.

Daisuke Arai
 */

#ifdef _MSC_VER
#include <stdint.h>
#define _STDINT_H
#include <windows.h>
#include <process.h>
#define pid_t int
#define getpid _getpid
#define sleep(n) Sleep(n * 1000)
#endif

#include <stdio.h>
#include <errno.h>
#ifndef _MSC_VER
#include <unistd.h>
#endif
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <jack/jack.h>
#include <jack/ringbuffer.h>
#include <libusb.h>
#include <signal.h>
#include <time.h>
#include "lasersharklib/lasershark_lib.h"


#define LASERSHARK_VIN 0x1fc9
#define LASERSHARK_PID 0x04d8

int use_isochronous = 0;
int do_exit = 0;
pid_t pid;

jack_client_t *client;

typedef jack_default_audio_sample_t sample_t;
typedef jack_nframes_t nframes_t;

jack_port_t *in_x;
jack_port_t *in_y;
jack_port_t *in_r;
jack_port_t *in_g;
jack_port_t *in_b;

nframes_t rate;

// Number of laserjack data packets the ringbuffer will have space for.
#define JACK_RB_PACKETS 256
jack_ringbuffer_t *jack_rb = NULL;
uint32_t jack_rb_len = 0;

uint8_t *laserjack_iso_data_packet_buf = NULL;
int laserjack_iso_data_packet_len = 0;

int lasershark_serialnum_len = 64;
unsigned char lasershark_serialnum[64];
uint32_t lasershark_fw_major_version = 0;
uint32_t lasershark_fw_minor_version = 0;


uint32_t lasershark_iso_packet_sample_count;
uint32_t lasershark_samp_element_count;
uint32_t lasershark_max_ilda_rate;
uint32_t lasershark_dac_min_val;
uint32_t lasershark_dac_max_val;

uint32_t lasershark_ringbuffer_sample_count;

uint32_t lasershark_ilda_rate = 0;

typedef struct
{
    float x, y, r, g, b;
} bufsample_t;

struct libusb_device_handle *devh_ctl = NULL;
struct libusb_device_handle *devh_data = NULL;
uint32_t max_iso_data_len = 0;

#ifndef _WIN32
sigset_t mask, oldmask;
#endif


static void sig_hdlr(int signum)
{
	printf("signum=%d\n", signum);
	fflush(stdout);
    switch (signum)
    {
#ifdef _WIN32
    case SIGTERM:
    case SIGABRT:
#endif
    case SIGINT:
        printf("\nSIGINT: Got request to quit\n");
        do_exit = 1;
        break;
#ifndef _WIN32
    case SIGUSR1:
        printf("sigusr1 caught\n");
        do_exit = 1;
        break;
#endif
    default:
        printf("what\n");
    }
}


void quit_program()
{
	printf("quit_program called\n");
	fflush(stdout);
#ifdef _WIN32
    do_exit = 1;
#else
    kill(pid, SIGUSR1);
#endif
}

/*
Internal callback for cleaning up async writes.
 */
void LIBUSB_CALL WriteAsyncCallback(struct libusb_transfer *transfer)
{
    if (transfer && (transfer->status != LIBUSB_TRANSFER_COMPLETED/* || transfer->actual_length != transfer->length*/))
    {
        printf("ISO transfer err: %d   bytes transferred: %d\n", transfer->status, transfer->actual_length);
    }
    free(transfer->buffer);
    libusb_free_transfer(transfer);
}


/*
Send data to laserdock device with bulk transfer.
Copied from LaserdockDevice.cpp in laserdockLib.
*/
boolean LaserdockDevice_send(unsigned char *data, uint32_t length) {
    int timeout_strikes = 3;

    int rv = 0; int transferred = 0;
    do {
        rv = libusb_bulk_transfer(devh_data, (3 | LIBUSB_ENDPOINT_OUT), data, length, &transferred, 0);
        if(rv==LIBUSB_ERROR_TIMEOUT){
            fprintf(stderr, "LaserdockDevice_send: Timedout\n");
            timeout_strikes--;
        }
    } while ( rv == LIBUSB_ERROR_TIMEOUT && timeout_strikes != 0);

    if (rv < 0) {
        return FALSE;
    }

    return TRUE;
}


/*
Writes an ISO packet to the Lasershark device.
Returns LASERSHARK_CMD_SUCCESS on success, LASERSHARK_CMD_FAIL on failure.
*/
int write_lasershark_data(unsigned char *data, int len)
{
    int rc;

	//printf("write_lasershark_data: len=%d\n", len);

    if (len > max_iso_data_len)
    {
        printf("Oversized iso write length. %d > %d\n", len, max_iso_data_len);
        return LASERSHARK_CMD_FAIL;
    }

    struct libusb_transfer *transfer = libusb_alloc_transfer(1);

    if (!transfer)
    {
        printf("Could not allocate memory for iso transfer.\n");
        return LIBUSB_ERROR_NO_MEM;
    }

    libusb_fill_iso_transfer(transfer, devh_data, (4 | LIBUSB_ENDPOINT_OUT), malloc(len), len, 1, &WriteAsyncCallback, 0, 0);

    if (!transfer->buffer)
    {
        printf("Could not create iso transfer packet.\n");
        libusb_free_transfer(transfer);
        return LASERSHARK_CMD_FAIL;
    }

    libusb_set_iso_packet_lengths(transfer, len);

    memcpy(transfer->buffer, data, len);

    rc = libusb_submit_transfer(transfer);

    if(rc != 0)
    {
        printf("Could not submit transfer: rc=%d\n", rc);
        return LASERSHARK_CMD_FAIL;
    }

    return LASERSHARK_CMD_SUCCESS;
}

int write_laserdock_data(unsigned char *data, int len)
{
    if (!LaserdockDevice_send(data, len))
        return LASERSHARK_CMD_FAIL;
    return LASERSHARK_CMD_SUCCESS;
}

/*
Converts from one linear scale to another in the sexiest manner possible, with math.
*/
static uint16_t convert(float OldValue, float OldMin, float OldMax, uint32_t NewMax, uint32_t NewMin)
{
    float v = (OldValue - OldMin) / (OldMax - OldMin);
    if (v > 1.0) v = 1.0;
    if (v < 0) v = 0.0;
    uint16_t val = (uint16_t)((v * (float)(NewMax - NewMin)) + (float)NewMin);
    return val;
}


/*
This function is only compatible with Lasershark V2.X modules. The format is a 16 byte (little endian) array of 4 elements
[0] = Channel A output (lower 12 bits), LASERSHARK_C_BITMASK field(0x4000), LASERSHARK_INTL_A_BITMASK(0x8000)
[1] = Channel B output (lower 12 bits)
[2] = X Galvo output (lower 12 bits)
[3] = Y Galvo output (lower 12 bits).

It's pretty messy... A version command should probably be added to the protocol and a check added so this can be used
with multiple different lasershark versions... but for now, it's good enough.

*/
static int process (nframes_t nframes, void *arg)
{
    uint16_t temp[6];
    int avail, written, i, j, rc;
    nframes_t frm;

    sample_t *i_x = (sample_t *) jack_port_get_buffer (in_x, nframes);
    sample_t *i_y = (sample_t *) jack_port_get_buffer (in_y, nframes);
    sample_t *i_r = (sample_t *) jack_port_get_buffer (in_r, nframes);
    sample_t *i_g = (sample_t *) jack_port_get_buffer (in_g, nframes);
    sample_t *i_b = (sample_t *) jack_port_get_buffer (in_b, nframes);

    // Read in all samples given to us from the GODLY JACK SERVER
    for (frm = 0; frm < nframes; frm++)
    {
        // Read the data and convert it to a format suitable to be sent out to the lasershark.
#if 1 /* LASERCUBE */
        temp[2] = convert(*i_x++ * -1, -1.0f, 1.0f, lasershark_dac_max_val, lasershark_dac_min_val);
#else
        temp[2] = convert(*i_x++, -1.0f, 1.0f, lasershark_dac_max_val, lasershark_dac_min_val);
#endif
        temp[3] = convert(*i_y++, -1.0f, 1.0f, lasershark_dac_max_val, lasershark_dac_min_val);
//         if (convert(*i_b++, -0.0f, 1.0f, lasershark_dac_max_val, lasershark_dac_min_val) >= 
//             (lasershark_dac_max_val + lasershark_dac_min_val)/2) {
//             temp[0] |= LASERSHARK_C_BITMASK; // If the laser power is >= half the dac output.. turn this ttl channel on.
//         }
//         temp[0] |= LASERSHARK_INTL_A_BITMASK; // Turn on the interlock pin since this is a valid sample.

        temp[0] = convert(*i_r++, 0.0f, 1.0f, 0xff, 0x00) | (convert(*i_g++, 0.0f, 1.0f, 0xff, 0x00) << 8);
        temp[1] = convert(*i_b++, 0.0f, 1.0f, 0xff, 0x00);
        //temp[3] = convert(*i_r++ * -1.0f, -1.0f, 1.0f, lasershark_dac_max_val, lasershark_dac_min_val);

        // Jam the samples in the ringbuffer.
        avail = jack_ringbuffer_write_space(jack_rb);
        if (avail >= lasershark_samp_element_count*sizeof(uint16_t))
        {
            written = jack_ringbuffer_write(jack_rb, (const char*)temp, lasershark_samp_element_count*sizeof(uint16_t));
            if (written != lasershark_samp_element_count*sizeof(uint16_t) )
            {
                printf("Ringbuffer write failure\n");
            }
        }
        else
        {
            printf("Ringbuffer full\n");
            break;
        }
    }

    // Send out as many data packets as we can to the DEMIGOD LASERSHARK DEVICE
    while ((i = jack_ringbuffer_read_space(jack_rb)) >= laserjack_iso_data_packet_len)
    {
        // read from the buffer
        j = jack_ringbuffer_read(jack_rb, (char *)laserjack_iso_data_packet_buf, laserjack_iso_data_packet_len);

        if (j != laserjack_iso_data_packet_len)
        {
            printf("Ringbuffer read failure\n");
            quit_program();
        }

        if (use_isochronous) {
            rc = write_lasershark_data((void *)laserjack_iso_data_packet_buf, laserjack_iso_data_packet_len);
        } else {
            rc = write_laserdock_data((void *)laserjack_iso_data_packet_buf, laserjack_iso_data_packet_len);
        }
        if (rc != LASERSHARK_CMD_SUCCESS)
        {
            quit_program();
        }
    }

    return 0;
}


static int bufsize (nframes_t nframes, void *arg)
{
    printf ("The maximum buffer size is now %u\n", nframes);
    return 0;
}


static int srate (nframes_t nframes, void *arg)
{
    rate = nframes;
    if (rate > lasershark_max_ilda_rate)
    {
        printf("Rate (%d) is higher than lasershark supports (%d)\n", rate, lasershark_max_ilda_rate);
        quit_program();
        return 1;
    }

    if (lasershark_ilda_rate != 0)
    {
        printf("ILDA rate was already set.. but we were asked to set again.. unimplemented! Dying.\n");
        quit_program();
        return 1;
    }

    lasershark_ilda_rate = nframes;
    printf ("ILDA rate specified as: %u pps\n", lasershark_ilda_rate);


    return 0;
}


static void jack_shutdown (void *arg)
{
    printf("JACK shutting down...\n");
    quit_program();
}


//bool LaserdockDeviceManagerPrivate::is_laserdock(libusb_device *device) const
boolean is_laserdock(libusb_device* device)
{
    struct libusb_device_descriptor device_descriptor;
    int result = libusb_get_device_descriptor(device, &device_descriptor);
    if (result < 0) {
        printf("Failed to get device descriptor!");
    }

    if (LASERSHARK_VIN == device_descriptor.idVendor && LASERSHARK_PID == device_descriptor.idProduct)
        return TRUE;

    return FALSE;
}

//LaserdockDeviceManagerPrivate::get_devices
libusb_device* find_laserdock()
{
    libusb_device **libusb_device_list;
    ssize_t cnt = libusb_get_device_list(NULL, &libusb_device_list);
    ssize_t i = 0;

    if (cnt < 0) {
        fprintf(stderr, "Error finding USB device\n");
        libusb_free_device_list(libusb_device_list, 1);
        return NULL;
    }

	libusb_device *temp = NULL;
    for (i = 0; i < cnt; i++) {
        libusb_device *dev = libusb_device_list[i];
        if (is_laserdock(dev)) {
			printf("Found laserdock device %p\n", dev);
			//return dev;
			temp = dev;
		}
    }

	if (temp != NULL) {
		return temp;
	}

	printf("Can not find laserdock device\n");
    libusb_free_device_list(libusb_device_list, cnt);

    return NULL;
}


int main (int argc, char *argv[])
{
    int rc;
    uint32_t temp;
#ifndef _WIN32
    struct sigaction sigact;
#endif

    const char jack_client_name[] = "lasershark";

    if (argc > 1 && strcmp(argv[1], "-i") == 0) {
        use_isochronous = 1;
    }

    pid = getpid();

#ifdef _WIN32
    signal(SIGINT, sig_hdlr);
    signal(SIGTERM, sig_hdlr);
    signal(SIGABRT, sig_hdlr);
#else
    sigact.sa_handler = sig_hdlr;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigaction(SIGINT, &sigact, NULL);
    sigaction(SIGUSR1, &sigact, NULL);
#endif

    rc = libusb_init(NULL);
    if (rc < 0)
    {
        fprintf(stderr, "Error initializing libusb: %d\n", /*libusb_error_name(rc)*/rc);
        exit(1);
    }

    libusb_set_debug(NULL, 3);

#if 0
    devh_ctl = libusb_open_device_with_vid_pid(NULL, LASERSHARK_VIN, LASERSHARK_PID);
    devh_data = libusb_open_device_with_vid_pid(NULL, LASERSHARK_VIN, LASERSHARK_PID);
    if (!devh_ctl || !devh_data)
    {
        fprintf(stderr, "Error finding USB device\n");
        goto out_post_release;
    }
#else
	libusb_device *device;
	device = find_laserdock();
	if (device == NULL)
		return 1;
	rc = libusb_open(device, &devh_ctl);
    if (rc != 0) goto out_post_release;
    rc = libusb_open(device, &devh_data);
    if (rc != 0) goto out_post_release;
#endif

    rc = libusb_claim_interface(devh_ctl, 0);
    if (rc < 0)
    {
        fprintf(stderr, "Error claiming control interface: %d\n", /*libusb_error_name(rc)*/rc);
        goto out_post_release;
    }
    rc = libusb_claim_interface(devh_data, 1);
    if (rc < 0)
    {
        fprintf(stderr, "Error claiming data interface: %d\n", /*libusb_error_name(rc)*/rc);
        libusb_release_interface(devh_ctl, 0);
        goto out_post_release;
    }

    if (use_isochronous) {
        rc = libusb_set_interface_alt_setting(devh_data, 1, 0);
        if (rc < 0)
        {
             fprintf(stderr, "Error setting alternative (ISO) data interface: %d\n", /*libusb_error_name(rc)*/rc);
             goto out;
        }
    } else {
        rc = libusb_set_interface_alt_setting(devh_data, 1, 1);
        if (rc < 0)
        {
             fprintf(stderr, "Error setting alternative (bulk) data interface: %d\n", /*libusb_error_name(rc)*/rc);
             goto out;
        }
    }

    struct libusb_device_descriptor desc;

    rc = libusb_get_device_descriptor(libusb_get_device(devh_ctl), &desc);
    if (rc < 0) {
        fprintf(stderr, "Error obtaining device descriptor: %d\n", /*libusb_error_name(rc)*/rc);
    }

    memset(lasershark_serialnum, 0, lasershark_serialnum_len);
    rc = libusb_get_string_descriptor_ascii(devh_ctl, desc.iSerialNumber, lasershark_serialnum, lasershark_serialnum_len);
    if (rc < 0) {
        fprintf(stderr, "Error obtaining iSerialNumber: %d\n", /*libusb_error_name(rc)*/rc);
    }

    printf("iSerialNumber: %s\n", lasershark_serialnum);


    rc = get_fw_major_version(devh_ctl, &lasershark_fw_major_version);
    if (rc != LASERSHARK_CMD_SUCCESS)
    {
        printf("Getting FW Major version failed. (Consider upgrading your firmware!)\n");
        goto out;
    }
    printf("Getting FW Major version: %d\n", lasershark_fw_major_version);

    rc = get_fw_minor_version(devh_ctl, &lasershark_fw_minor_version);
    if (rc != LASERSHARK_CMD_SUCCESS)
    {
        printf("Getting FW Minor version failed. (Consider upgrading your firmware!)\n");
        goto out;
    }
    printf("Getting FW Minor version: %d\n", lasershark_fw_minor_version);

    if (lasershark_fw_minor_version != LASERSHARK_FW_MAJOR_VERSION) {
        printf("Your FW is not capable of proper bulk transfers or clear commands. Consider upgrading your firmware!\n");
    } else {
        printf("Firmware supports ring buffer clears. Clearing now.\n");
        //rc = clear_ringbuffer(devh_ctl);
        //if (rc != LASERSHARK_CMD_SUCCESS) {
        //    printf("Clearing ringbuffer buffer failed.\n");
        //    goto out;
        //}
    }

    max_iso_data_len = libusb_get_max_iso_packet_size(libusb_get_device(devh_data), (4 | LIBUSB_ENDPOINT_OUT));
    printf("Max iso data packet length according to descriptors: %d\n", max_iso_data_len);


    rc = get_samp_element_count(devh_ctl, &lasershark_samp_element_count);
    if (rc != LASERSHARK_CMD_SUCCESS)
    {
        printf("Getting sample element count failed\n");
        goto out;
    }
    printf("Getting sample element count: %d\n", lasershark_samp_element_count);


    rc = get_iso_packet_sample_count(devh_ctl, &lasershark_iso_packet_sample_count);
    if (rc != LASERSHARK_CMD_SUCCESS)
    {
        printf("Getting iso packet sample count failed\n");
        goto out;
    }
    printf("Getting iso packet sample count: %d\n", lasershark_iso_packet_sample_count);


    rc = get_max_ilda_rate(devh_ctl, &lasershark_max_ilda_rate);
    if (rc != LASERSHARK_CMD_SUCCESS)
    {
        printf("Getting max ilda rate failed\n");
        goto out;
    }
    printf("Getting max ilda rate: %u pps\n", lasershark_max_ilda_rate);


    rc = get_dac_min(devh_ctl, &lasershark_dac_min_val);
    if (rc != LASERSHARK_CMD_SUCCESS)
    {
        printf("Getting dac min failed\n");
        goto out;
    }
    printf("Getting dac min: %d\n", lasershark_dac_min_val);


    rc = get_dac_max(devh_ctl, &lasershark_dac_max_val);
    if (rc != LASERSHARK_CMD_SUCCESS) {
        printf("Getting dac max failed\n");
        goto out;
    }
    printf("getting dac max: %d\n", lasershark_dac_max_val);


    rc = get_ringbuffer_sample_count(devh_ctl, &lasershark_ringbuffer_sample_count);
    if (rc != LASERSHARK_CMD_SUCCESS)
    {
        printf("Getting ringbuffer sample count\n");
        goto out;
    }
    printf("Getting ringbuffer sample count: %d\n", lasershark_ringbuffer_sample_count);


    rc = get_ringbuffer_empty_sample_count(devh_ctl, &temp);
    if (rc != LASERSHARK_CMD_SUCCESS)
    {
        printf("Getting ringbuffer empty sample count failed. (Consider upgrading your firmware)\n");
    }
    printf("Getting ringbuffer empty sample count: %d\n", temp);


    jack_status_t jack_status;
    jack_options_t  jack_options = JackNullOption;

    if ((client = jack_client_open(jack_client_name, jack_options, &jack_status)) == 0)
    {
        fprintf (stderr, "JACK server not running? FSCK!\n");
        goto out;
    }


    jack_set_process_callback (client, process, 0);
    jack_set_buffer_size_callback (client, bufsize, 0);
    jack_set_sample_rate_callback (client, srate, 0);
    jack_on_shutdown (client, jack_shutdown, 0);

    in_x = jack_port_register (client, "in_x", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
    in_y = jack_port_register (client, "in_y", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
#if 1 /* FIX PORT_NAME for LASERCUBE */
    in_r = jack_port_register (client, "in_r", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
    in_g = jack_port_register (client, "in_g", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
#else
    in_r = jack_port_register (client, "in_g", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
    in_g = jack_port_register (client, "in_r", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
#endif
    in_b = jack_port_register (client, "in_b", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);

    if (lasershark_ilda_rate == 0)
    {
        printf("ILDA rate wasn't specified by server.. unimplemented case.. dying\n");
        goto out;
    }

    rc = set_ilda_rate(devh_ctl, lasershark_ilda_rate);
    if (rc != LASERSHARK_CMD_SUCCESS)
    {
        printf("setting ILDA rate failed\n");
        goto out;
    }
    printf("Setting ILDA rate worked: %u pps\n", lasershark_ilda_rate);


    rc = set_output(devh_ctl, LASERSHARK_CMD_OUTPUT_ENABLE);
    if (rc != LASERSHARK_CMD_SUCCESS)
    {
        printf("Enable output failed\n");
        goto out;
    }
    printf("Enable output worked\n");


    laserjack_iso_data_packet_len = lasershark_iso_packet_sample_count * lasershark_samp_element_count * sizeof(uint16_t);
    laserjack_iso_data_packet_buf = malloc(laserjack_iso_data_packet_len);
    if (laserjack_iso_data_packet_buf == NULL)
    {
        printf("Could not allocate laserjack iso data packet buffer\n");
        goto out;
    }

    jack_rb_len = laserjack_iso_data_packet_len * JACK_RB_PACKETS;
    jack_rb = jack_ringbuffer_create(jack_rb_len);
    if (jack_rb == NULL)
    {
        printf("Could not allocate JACK ringbuffer\n");
        goto out;
    }


    // lock the buffer into memory, this is *NOT* realtime safe, do it before
    // using the buffer!
    rc = jack_ringbuffer_mlock(jack_rb);
    if (rc)
    {
        printf("Could not lock JACK ringbuffer memory\n");
        goto out;
    }

    if (jack_activate (client))
    {
        fprintf (stderr, "Cannot activate JACK client");
        goto out;
    }

#ifndef _WIN32
    sigemptyset (&mask);
    sigaddset (&mask, SIGUSR1);

    sigprocmask (SIG_BLOCK, &mask, &oldmask);
#endif

    printf("Running (RGB corrected version)\n");
    fflush(stdout);
    do
    {
#if 1
		libusb_handle_events(0); // Quick hack.. this should be made better later.
#else
		struct timeval tv = { 1, 0 };
		rc = libusb_handle_events_timeout_completed(NULL, &tv, NULL);
		if (rc < 0) {
			quit_program();
			break;
		}
#endif
#if !defined(_WIN32)
        sigsuspend(&oldmask);
        printf("Looping... (Must have recieved a signal, don't panic).\n");
        fflush(stdout);
#endif
    }
    while (!do_exit);
#if !defined(_WIN32)
    sigprocmask (SIG_UNBLOCK, &mask, NULL);
#endif

    printf("Quitting gracefully\n");

// THINGS COME HERE TO DIE!!!!!!!!!!!!!!!!!!!
out:
    libusb_release_interface(devh_ctl, 0);
    libusb_release_interface(devh_data, 0);

out_post_release:
    if (devh_ctl)
    {
        libusb_close(devh_ctl);
    }
    if (devh_data)
    {
        libusb_close(devh_data);
    }
    libusb_exit(NULL);

    if (jack_rb != NULL)
    {
        jack_ringbuffer_free(jack_rb);
    }

    if (laserjack_iso_data_packet_buf != NULL)
    {
        free(laserjack_iso_data_packet_buf);
    }


    return rc;
}
