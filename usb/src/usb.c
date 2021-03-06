/*
 *  M-Stack USB Device Stack Implementation
 *  Copyright (C) 2013 Alan Ott <alan@signal11.us>
 *  Copyright (C) 2013 Signal 11 Software
 *
 *  Initial version for PIC18, 2008-02-24
 *  PIC24 port, 2013-04-08
 *
 *  M-Stack is free software: you can redistribute it and/or modify it under
 *  the terms of the GNU Lesser General Public License as published by the
 *  Free Software Foundation, version 3
 *
 *  M-Stack is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 *  License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this software.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef __XC16__
#include <libpic30.h>
#include <xc.h>
#elif __C18
#include <p18f4550.h>
#include <delays.h>
#elif __XC8
#include <xc.h>
#endif

#include <string.h>

#include "usb_config.h"
#include "usb.h"
#include "usb_hal.h"
#include "usb_ch9.h"

#define MIN(x,y) (((x)<(y))?(x):(y))

/* Even though they're the same, It's convenient below (for the buffer
 * macros) to have separate #defines for IN and OUT EP 0 lengths which
 * match the format of the other endpoint length #defines. */
#define EP_0_OUT_LEN EP_0_LEN
#define EP_0_IN_LEN  EP_0_LEN

STATIC_SIZE_CHECK_EQUAL(sizeof(struct endpoint_descriptor), 7);
STATIC_SIZE_CHECK_EQUAL(sizeof(struct hid_descriptor), 9);
STATIC_SIZE_CHECK_EQUAL(sizeof(struct interface_descriptor), 9);
STATIC_SIZE_CHECK_EQUAL(sizeof(struct configuration_descriptor), 9);
STATIC_SIZE_CHECK_EQUAL(sizeof(struct device_descriptor), 18);
STATIC_SIZE_CHECK_EQUAL(sizeof(struct setup_packet), 8);
STATIC_SIZE_CHECK_EQUAL(sizeof(struct buffer_descriptor), 4);

struct buffer_descriptor_pair {
	struct buffer_descriptor ep_out;
	struct buffer_descriptor ep_in;
};

#ifdef __C18
/* The buffer descriptors. Per the PIC18F4550 Data sheet, when _not_ using
   ping-pong buffering, these must be laid out sequentially starting at
   address 0x0400 in the following order, ep0_out, ep0_in,ep1_out, ep1_in,
   etc. These must be initialized prior to use. */
#pragma udata buffer_descriptors=BD_ADDR
#endif
static struct buffer_descriptor_pair bds[NUM_ENDPOINT_NUMBERS+1] BD_ATTR_TAG;

#ifdef __C18
/* The actual buffers to and from which the data is transferred from the SIE
   (from the USB bus). These buffers must fully be located between addresses
   0x400 and 0x7FF per the datasheet.*/
/* This addr is for the PIC18F4550 */
#pragma udata usb_buffers=0x500
#elif __XC16__
	/* Buffers can go anywhere on PIC24 parts which are supported (so far). */
#elif __XC8
	/* Addresses are set by BD_ADDR and BUF_ADDR below. */
#else
	#error compiler not supported
#endif

static struct {
#define EP_BUF(n) \
	unsigned char ep_##n##_out_buf[EP_##n##_OUT_LEN]; \
	unsigned char ep_##n##_in_buf[EP_##n##_IN_LEN];

#if NUM_ENDPOINT_NUMBERS >= 0
	EP_BUF(0)
#endif
#if NUM_ENDPOINT_NUMBERS >= 1
	EP_BUF(1)
#endif
#if NUM_ENDPOINT_NUMBERS >= 2
	EP_BUF(2)
#endif
#if NUM_ENDPOINT_NUMBERS >= 3
	EP_BUF(3)
#endif
#if NUM_ENDPOINT_NUMBERS >= 4
	EP_BUF(4)
#endif
#if NUM_ENDPOINT_NUMBERS >= 5
	EP_BUF(5)
#endif
#if NUM_ENDPOINT_NUMBERS >= 6
	EP_BUF(6)
#endif
#if NUM_ENDPOINT_NUMBERS >= 7
	EP_BUF(7)
#endif
#if NUM_ENDPOINT_NUMBERS >= 8
	EP_BUF(8)
#endif
#if NUM_ENDPOINT_NUMBERS >= 9
	EP_BUF(9)
#endif
#if NUM_ENDPOINT_NUMBERS >= 10
	EP_BUF(10)
#endif
#if NUM_ENDPOINT_NUMBERS >= 11
	EP_BUF(11)
#endif
#if NUM_ENDPOINT_NUMBERS >= 12
	EP_BUF(12)
#endif
#if NUM_ENDPOINT_NUMBERS >= 13
	EP_BUF(13)
#endif
#if NUM_ENDPOINT_NUMBERS >= 14
	EP_BUF(14)
#endif
#if NUM_ENDPOINT_NUMBERS >= 15
	EP_BUF(15)
#endif

#undef EP_BUF
} ep_buffers XC8_BUFFER_ADDR_TAG;

struct ep_buf {
	unsigned char * const out;
	unsigned char * const in;
	const uint8_t out_len;
	const uint8_t in_len;

#define EP_OUT_HALT_FLAG 0x1
#define EP_IN_HALT_FLAG 0x2
	uint8_t flags;
};

#ifdef __C18
#pragma idata
#endif

#define EP_BUFS(n) { ep_buffers.ep_##n##_out_buf, ep_buffers.ep_##n##_in_buf, EP_##n##_OUT_LEN, EP_##n##_IN_LEN },

static struct ep_buf ep_buf[NUM_ENDPOINT_NUMBERS+1] = {
#if NUM_ENDPOINT_NUMBERS >= 0
	EP_BUFS(0)
#endif
#if NUM_ENDPOINT_NUMBERS >= 1
	EP_BUFS(1)
#endif
#if NUM_ENDPOINT_NUMBERS >= 2
	EP_BUFS(2)
#endif
#if NUM_ENDPOINT_NUMBERS >= 3
	EP_BUFS(3)
#endif
#if NUM_ENDPOINT_NUMBERS >= 4
	EP_BUFS(4)
#endif
#if NUM_ENDPOINT_NUMBERS >= 5
	EP_BUFS(5)
#endif
#if NUM_ENDPOINT_NUMBERS >= 6
	EP_BUFS(6)
#endif
#if NUM_ENDPOINT_NUMBERS >= 7
	EP_BUFS(7)
#endif
#if NUM_ENDPOINT_NUMBERS >= 8
	EP_BUFS(8)
#endif
#if NUM_ENDPOINT_NUMBERS >= 9
	EP_BUFS(9)
#endif
#if NUM_ENDPOINT_NUMBERS >= 10
	EP_BUFS(10)
#endif
#if NUM_ENDPOINT_NUMBERS >= 11
	EP_BUFS(11)
#endif
#if NUM_ENDPOINT_NUMBERS >= 12
	EP_BUFS(12)
#endif
#if NUM_ENDPOINT_NUMBERS >= 13
	EP_BUFS(13)
#endif
#if NUM_ENDPOINT_NUMBERS >= 14
	EP_BUFS(14)
#endif
#if NUM_ENDPOINT_NUMBERS >= 15
	EP_BUFS(15)
#endif

};
#undef EP_BUFS

/* Global data */
static bool addr_pending;
static uint8_t addr;
static uint8_t g_configuration;
static bool control_need_zlp;
static bool returning_short;

/* Data associated with multi-packet control transfers */
static usb_ep0_data_stage_callback ep0_data_stage_callback;
static char   *ep0_data_stage_in_buffer; /* XC8 v1.12 fails if this is const on PIC16 */
static char   *ep0_data_stage_out_buffer;
static size_t  ep0_data_stage_buf_remaining;
static void   *ep0_data_stage_context;
static uint8_t ep0_data_stage_direc; /*1=IN, 0=OUT, Same as USB spec.*/

static void reset_ep0_data_stage()
{
	ep0_data_stage_in_buffer = NULL;
	ep0_data_stage_out_buffer = NULL;
	ep0_data_stage_buf_remaining = 0;

	/* There's no need to reset the following because no decisions are
	   made based on them:
	     ep0_data_stage_callback,
	     ep0_data_stage_context,
	     ep0_data_stage_direc
	 */
}

#define SERIAL(x)
#define SERIAL_VAL(x)

/* usb_init() is called at powerup time, and when the device gets
   the reset signal from the USB bus (D+ and D- both held low) indicated
   by interrput bit URSTIF. */
void usb_init(void)
{
	uint8_t i;

	/* Initialize the USB. 18.4 of PIC24FJ64GB004 datasheet */
	SET_PING_PONG_MODE(0);   /* 0 = disable ping-pong buffer */
	SFR_USB_INTERRUPT_EN = 0x0;
	SFR_USB_EXTENDED_INTERRUPT_EN = 0x0;
	
	SFR_USB_EN = 1; /* enable USB module */

#ifdef USE_OTG
	SFR_OTGEN = 1;
#endif

	
#ifdef NEEDS_PULL
	SFR_PULL_EN = 1;  /* pull-up enable */
#endif

#ifdef HAS_ON_CHIP_XCVR_DIS
	SFR_ON_CHIP_XCVR_DIS = 0; /* on-chip transceiver Disable */
#endif

#ifdef HAS_LOW_SPEED
	SFR_FULL_SPEED_EN = 1;   /* Full-speed enable */
#endif

	CLEAR_USB_TOKEN_IF();   /* Clear 4 times to clear out USTAT FIFO */
	CLEAR_USB_TOKEN_IF();
	CLEAR_USB_TOKEN_IF();
	CLEAR_USB_TOKEN_IF();

	CLEAR_ALL_USB_IF();

#ifdef USB_USE_INTERRUPTS
	SFR_TRANSFER_IE = 1; /* USB Transfer Interrupt Enable */
	SFR_STALL_IE = 1;    /* USB Stall Interrupt Enable */
	SFR_RESET_IE = 1;    /* USB Reset Interrupt Enable */
#ifdef START_OF_FRAME_CALLBACK
	SFR_SOF_IE = 1;      /* USB Start-Of-Frame Interrupt Enable */
#endif
#endif

#ifdef USB_NEEDS_SET_BD_ADDR_REG
	union WORD {
		struct {
			uint8_t lb;
			uint8_t hb;
		};
		uint16_t w;
		void *ptr;
	};
	union WORD w;
	w.ptr = bds;

	SFR_BD_ADDR_REG = w.hb;
#endif

	/* These are the UEP/U1EP endpoint management registers. */
	
	/* Clear them all out. This is important because a bootloader
	   could have set them to non-zero */
	memset((void*)&SFR_EP_MGMT(0), 0x0, sizeof(SFR_EP_MGMT(0)) * 16);
	
	/* Set up Endpoint zero */
	SFR_EP_MGMT(0).SFR_EP_MGMT_HANDSHAKE = 1; /* Endpoint handshaking enable */
	SFR_EP_MGMT(0).SFR_EP_MGMT_CON_DIS = 0; /* 1=Disable control operations */
	SFR_EP_MGMT(0).SFR_EP_MGMT_OUT_EN = 1; /* Endpoint Out Transaction Enable */
	SFR_EP_MGMT(0).SFR_EP_MGMT_IN_EN = 1; /* Endpoint In Transaction Enable */
	SFR_EP_MGMT(0).SFR_EP_MGMT_STALL = 0; /* Stall */

	for (i = 1; i <= NUM_ENDPOINT_NUMBERS; i++) {
		volatile SFR_EP_MGMT_TYPE *ep = &SFR_EP_MGMT(1) + (i-1);
		ep->SFR_EP_MGMT_HANDSHAKE = 1; /* Endpoint handshaking enable */
		ep->SFR_EP_MGMT_CON_DIS = 1; /* 1=Disable control operations */
		ep->SFR_EP_MGMT_OUT_EN = 1; /* Endpoint Out Transaction Enable */
		ep->SFR_EP_MGMT_IN_EN = 1; /* Endpoint In Transaction Enable */
		ep->SFR_EP_MGMT_STALL = 0; /* Stall */
	}

	/* Reset the Address. */
	SFR_USB_ADDR = 0x0;
	addr_pending = 0;
	g_configuration = 0;
	for (i = 0; i <= NUM_ENDPOINT_NUMBERS; i++)
		ep_buf[i].flags = 0;

	memset(bds, 0x0, sizeof(bds));

	/* Setup endpoint 0 Output buffer descriptor.
	   Input and output are from the HOST perspective. */
	bds[0].ep_out.BDnADR = (BDNADR_TYPE) ep_buf[0].out;
	SET_BDN(bds[0].ep_out, BDNSTAT_UOWN, ep_buf[0].out_len);

	/* Setup endpoint 0 Input buffer descriptor.
	   Input and output are from the HOST perspective. */
	bds[0].ep_in.BDnADR = (BDNADR_TYPE) ep_buf[0].in;
	SET_BDN(bds[0].ep_in, 0, ep_buf[0].in_len);

	for (i = 1; i <= NUM_ENDPOINT_NUMBERS; i++) {
		/* Setup endpoint 1 Output buffer descriptor.
		   Input and output are from the HOST perspective. */
		bds[i].ep_out.BDnADR = (BDNADR_TYPE) ep_buf[i].out;
		SET_BDN(bds[i].ep_out, BDNSTAT_UOWN, ep_buf[i].out_len);

		/* Setup endpoint 1 Input buffer descriptor.
		   Input and output are from the HOST perspective. */
		bds[i].ep_in.BDnADR = (BDNADR_TYPE) ep_buf[i].in;
		SET_BDN(bds[i].ep_in, BDNSTAT_DTS, ep_buf[i].in_len);
	}
	
	#ifdef USB_NEEDS_POWER_ON
	SFR_USB_POWER = 1;
	#endif

#ifdef USE_OTG
	SFR_DPPULUP = 1;
#endif

	reset_ep0_data_stage();

#ifdef USB_USE_INTERRUPTS
	SFR_USB_IE = 1;     /* USB Interrupt enable */
#endif
	
	//UIRbits.URSTIF = 0; /* Clear USB Reset on Start */
}

static void reset_bd0_out(void)
{
	/* Clean up the Buffer Descriptors.
	 * Set the length and hand it back to the SIE.
	 * The Address stays the same. */
	SET_BDN(bds[0].ep_out, BDNSTAT_UOWN, ep_buf[0].out_len);
}

static void stall_ep0(void)
{
	/* Stall Endpoint 0. It's important that DTSEN and DTS are zero. */
	SET_BDN(bds[0].ep_in, BDNSTAT_UOWN|BDNSTAT_BSTALL, ep_buf[0].in_len);
}

static void stall_ep_in(uint8_t ep)
{
	/* Stall Endpoint. It's important that DTSEN and DTS are zero. */
	SET_BDN(bds[ep].ep_in, BDNSTAT_UOWN|BDNSTAT_BSTALL, ep_buf[ep].in_len);
}

static void stall_ep_out(uint8_t ep)
{
	/* Stall Endpoint. It's important that DTSEN and DTS are zero. */
	SET_BDN(bds[ep].ep_out, BDNSTAT_UOWN|BDNSTAT_BSTALL , ep_buf[ep].out_len);
}

static void send_zero_length_packet_ep0()
{
	bds[0].ep_in.STAT.BDnSTAT = 0;
	SET_BDN(bds[0].ep_in, BDNSTAT_UOWN|BDNSTAT_DTS|BDNSTAT_DTSEN, 0);
}

/* Start Control Return
 *
 * Start the data stage of an IN control transfer. This is primarily used
 * for sending descriptors and other chapter 9 data back to the host, but it
 * is also called from usb_send_data_stage() for handling control transfers
 * handled by the application.
 *
 * This function sets up the global state variables necessary to do a
 * multi-transaction IN data stage and sends the first transaction.
 *
 * Params:
 *   ptr             - a pointer to the data to send
 *   len             - the size of the data which can be sent (ie: the size
 *                     of the entire descriptor)
 *   bytes_asked_for - the number of bytes asked for by the host in
 *                     the SETUP phase
 */
static void start_control_return(const void *ptr, size_t len, size_t bytes_asked_for)
{
	uint8_t bytes_to_send = MIN(len, EP_0_IN_LEN);
	bytes_to_send = MIN(bytes_to_send, bytes_asked_for);
	returning_short = len != bytes_asked_for;
	memcpy_from_rom(ep_buf[0].in, ptr, bytes_to_send);
	ep0_data_stage_in_buffer = ((char*)ptr) + bytes_to_send;
	ep0_data_stage_buf_remaining = MIN(bytes_asked_for, len) - bytes_to_send;

	/* Send back the first transaction */
	bds[0].ep_in.STAT.BDnSTAT = 0;
	SET_BDN(bds[0].ep_in,
		BDNSTAT_UOWN|BDNSTAT_DTS|BDNSTAT_DTSEN,
		bytes_to_send);
}

static inline int8_t handle_standard_control_request()
{
	FAR struct setup_packet *setup = (struct setup_packet*) ep_buf[0].out;
	int8_t res = 0;

	if (setup->bRequest == GET_DESCRIPTOR) {
		char descriptor = ((setup->wValue >> 8) & 0x00ff);
		uint8_t descriptor_index = setup->wValue & 0x00ff;

		if (descriptor == DESC_DEVICE) {
			SERIAL("Get Descriptor for DEVICE");

			/* Return Device Descriptor */
			start_control_return(&USB_DEVICE_DESCRIPTOR, USB_DEVICE_DESCRIPTOR.bLength, setup->wLength);
		}
		else if (descriptor == DESC_CONFIGURATION) {
			const struct configuration_descriptor *desc;
			if (descriptor_index >= NUMBER_OF_CONFIGURATIONS)
				stall_ep0();
			else {
				desc = USB_CONFIG_DESCRIPTOR_MAP[descriptor_index];
				start_control_return(desc, desc->wTotalLength, setup->wLength);
			}
		}
		else if (descriptor == DESC_STRING) {
#ifdef USB_STRING_DESCRIPTOR_FUNC
			const void *desc;
			int16_t len;

			len = USB_STRING_DESCRIPTOR_FUNC(descriptor_index, &desc);
			if (len < 0) {
				stall_ep0();
				SERIAL("Unsupported string descriptor requested");
			}
			else
				start_control_return(desc, len, setup->wLength);
#else
			/* Strings are not supported on this device. */
			stall_ep0();
#endif
		}
		else {
#ifdef UNKNOWN_GET_DESCRIPTOR_CALLBACK
			int16_t len;
			const void *desc;
			len = UNKNOWN_GET_DESCRIPTOR_CALLBACK(setup, &desc);
			if (len < 0) {
				stall_ep0();
				SERIAL("Unsupported descriptor requested");
			}
			else
				start_control_return(desc, len, setup->wLength);
#else
			/* Unknown Descriptor. Stall the endpoint. */
			stall_ep0();
			SERIAL("Unknown Descriptor");
			SERIAL_VAL(descriptor);
#endif
		}
	}
	else if (setup->bRequest == SET_ADDRESS) {
		/* Mark the ADDR as pending. The address gets set only
		   after the transaction is complete. */
		addr_pending = 1;
		addr = setup->wValue;

		send_zero_length_packet_ep0();
	}
	else if (setup->bRequest == SET_CONFIGURATION) {
		/* Set the configuration. wValue is the configuration.
		 * A value of 0 means to un-set the configuration and
		 * go back to the ADDRESS state. */
		uint8_t req = setup->wValue & 0x00ff;
#ifdef SET_CONFIGURATION_CALLBACK
		SET_CONFIGURATION_CALLBACK(req);
#endif
		send_zero_length_packet_ep0();
		g_configuration = req;

		SERIAL("Set configuration to");
		SERIAL_VAL(req);
	}
	else if (setup->bRequest == GET_CONFIGURATION) {
		/* Return the current Configuration. */

		SERIAL("Get Configuration. Returning:");
		SERIAL_VAL(g_configuration);

		bds[0].ep_in.STAT.BDnSTAT = 0;
		ep_buf[0].in[0] = g_configuration;
		SET_BDN(bds[0].ep_in,
			BDNSTAT_UOWN|BDNSTAT_DTS|BDNSTAT_DTSEN, 1);
	}
	else if (setup->bRequest == GET_STATUS) {

		SERIAL("Get Status (dst, index):");
		SERIAL_VAL(setup->REQUEST.destination);
		SERIAL_VAL(setup->wIndex);

		if (setup->REQUEST.destination == 0 /*0=device*/) {
			/* Status for the DEVICE requested
			   Return as a single byte in the return packet. */
			bds[0].ep_in.STAT.BDnSTAT = 0;
#ifdef GET_DEVICE_STATUS_CALLBACK
			*((uint16_t*)ep_buf[0].in) = GET_DEVICE_STATUS_CALLBACK();
#else
			ep_buf[0].in[0] = 0;
			ep_buf[0].in[1] = 0;
#endif
			SET_BDN(bds[0].ep_in,
				BDNSTAT_UOWN|BDNSTAT_DTS|BDNSTAT_DTSEN, 2);
		}
		else if (setup->REQUEST.destination == 2 /*2=endpoint*/) {
			/* Status of endpoint */
			uint8_t ep_num = setup->wIndex & 0x0f;
			if (ep_num <= NUM_ENDPOINT_NUMBERS) {
				uint8_t flags = ep_buf[ep_num].flags;
				bds[0].ep_in.STAT.BDnSTAT = 0;
				ep_buf[0].in[0] = ((setup->wIndex & 0x80) ?
					flags & EP_IN_HALT_FLAG :
					flags & EP_OUT_HALT_FLAG) != 0;
				ep_buf[0].in[1] = 0;
				SET_BDN(bds[0].ep_in,
					BDNSTAT_UOWN|BDNSTAT_DTS|BDNSTAT_DTSEN,
					2);
			}
			else {
				/* Endpoint doesn't exist. STALL. */
				stall_ep0();
			}
		}
		else {
			stall_ep0();
			SERIAL("Stalling. Status Requested for destination:");
			SERIAL_VAL(setup->REQUEST.destination);
		}

	}
	else if (setup->bRequest == SET_INTERFACE) {
		/* Set the alternate setting for an interface.
		 * wIndex is the interface.
		 * wValue is the alternate setting. */
#ifdef SET_INTERFACE_CALLBACK
		int8_t res;
		res = SET_INTERFACE_CALLBACK(setup->wIndex, setup->wValue);
		if (res < 0) {
			stall_ep0();
		}
		else
			send_zero_length_packet_ep0();
#else
		/* If there's no callback, then assume that
		 * we only have one alternate setting per
		 * interface. */
		send_zero_length_packet_ep0();
#endif
	}
	else if (setup->bRequest == GET_INTERFACE) {
		SERIAL("Get Interface");
		SERIAL_VAL(setup->bRequest);
		SERIAL_VAL(setup->REQUEST.destination);
		SERIAL_VAL(setup->REQUEST.type);
		SERIAL_VAL(setup->REQUEST.direction);
#ifdef GET_INTERFACE_CALLBACK
		int8_t res = GET_INTERFACE_CALLBACK(setup->wIndex);
		if (res < 0)
			stall_ep0();
		else {
			/* Return the current alternate setting
			   as a single byte in the return packet. */
			bds[0].ep_in.STAT.BDnSTAT = 0;
			ep_buf[0].in[0] = res;
			SET_BDN(bds[0].ep_in,
				BDNSTAT_UOWN|BDNSTAT_DTS|BDNSTAT_DTSEN, 1);
		}
#else
		/* If there's no callback, then assume that
		 * we only have one alternate setting per
		 * interface and return zero as that
		 * alternate setting. */
		bds[0].ep_in.STAT.BDnSTAT = 0;
		ep_buf[0].in[0] = 0;
		SET_BDN(bds[0].ep_in,
			BDNSTAT_UOWN|BDNSTAT_DTS|BDNSTAT_DTSEN, 1);
#endif
	}
	else if (setup->bRequest == CLEAR_FEATURE || setup->bRequest == SET_FEATURE) {
		uint8_t stall = 1;
		if (setup->REQUEST.destination == 0/*0=device*/) {
			SERIAL("Set/Clear feature for device");
			/* TODO Remote Wakeup flag */
		}

		if (setup->REQUEST.destination == 2/*2=endpoint*/) {
			if (setup->wValue == 0/*0=ENDPOINT_HALT*/) {
				uint8_t ep_num = setup->wIndex & 0x0f;
				uint8_t ep_dir = setup->wIndex & 0x80;
				if (ep_num <= NUM_ENDPOINT_NUMBERS) {
					if (setup->bRequest == SET_FEATURE) {
						/* Set Endpoint Halt Feature.
						   Stall the affected endpoint. */
						if (ep_dir) {
							ep_buf[ep_num].flags |= EP_IN_HALT_FLAG;
							stall_ep_in(ep_num);
						}
						else {
							ep_buf[ep_num].flags |= EP_OUT_HALT_FLAG;
							stall_ep_out(ep_num);
						}
					}
					else {
						/* Clear Endpoint Halt Feature.
						   Clear the STALL on the affected endpoint. */
						if (ep_dir) {
							ep_buf[ep_num].flags &= ~(EP_IN_HALT_FLAG);
							SET_BDN(bds[ep_num].ep_in, BDNSTAT_DTS, ep_buf[ep_num].in_len);
						}
						else {
							ep_buf[ep_num].flags &= ~(EP_OUT_HALT_FLAG);
							SET_BDN(bds[ep_num].ep_out, BDNSTAT_UOWN, ep_buf[ep_num].out_len);
						}
					}
#ifdef ENDPOINT_HALT_CALLBACK
					ENDPOINT_HALT_CALLBACK(setup->wIndex, (setup->bRequest == SET_FEATURE));
#endif
					stall = 0;
				}
			}
		}

		if (!stall) {
			send_zero_length_packet_ep0();
		}
		else
			stall_ep0();
	}
	else {
		res = -1;

		SERIAL("unsupported request (req, dest, type, dir) ");
		SERIAL_VAL(setup->bRequest);
		SERIAL_VAL(setup->REQUEST.destination);
		SERIAL_VAL(setup->REQUEST.type);
		SERIAL_VAL(setup->REQUEST.direction);
	}

	return res;
}

static inline void handle_ep0_setup()
{
	FAR struct setup_packet *setup = (struct setup_packet*) ep_buf[0].out;
	ep0_data_stage_direc = setup->REQUEST.direction;
	int8_t res;

	if (ep0_data_stage_buf_remaining) {
		/* A SETUP transaction has been received while waiting
		 * for a DATA stage to complete; something is broken.
		 * If this was an application-controlled transfer (and
		 * there's a callback), notify the application of this. */
		if (ep0_data_stage_callback)
			ep0_data_stage_callback(0/*fail*/, ep0_data_stage_context);

		reset_ep0_data_stage();
	}

	if (setup->REQUEST.type == REQUEST_TYPE_STANDARD) {
		res = handle_standard_control_request();
		if (res < 0)
			goto handle_unknown;
	}
	else
		goto handle_unknown;

	goto out;

handle_unknown:

#ifdef UNKNOWN_SETUP_REQUEST_CALLBACK
	res = UNKNOWN_SETUP_REQUEST_CALLBACK(setup);
	if (res < 0)
		stall_ep0();
	else {
		/* If the application has handled this request, it
		 * will have already set up whatever needs to be set
		 * up for the data stage. */
	}
#else
	/* Unsupported Request. Stall the Endpoint. */
	stall_ep0();
#endif

out:
	/* SETUP packet sets PKTDIS which disables
	 * future SETUP packet reception. Turn it off
	 * afer we've processed the current SETUP
	 * packet to avoid a race condition. */
	SFR_USB_PKT_DIS = 0;
}

static inline void handle_ep0_out()
{
	uint8_t pkt_len = BDN_LENGTH(bds[0].ep_out);
	if (ep0_data_stage_direc == 1/*1=IN*/) {
		/* An empty OUT packet on an IN control transfer
		 * means the STATUS stage of the control
		 * transfer has completed (possibly early). */

		/* Notify the application (if applicable) */
		if (ep0_data_stage_callback)
			ep0_data_stage_callback(1/*true*/, ep0_data_stage_context);
		reset_ep0_data_stage();

		/* Clean up the Buffer Descriptors.
		   Set the length and hand it back to the SIE. */
		bds[0].ep_out.STAT.BDnSTAT = 0;
		SET_BDN(bds[0].ep_out,
			BDNSTAT_UOWN|BDNSTAT_DTS|BDNSTAT_DTSEN,
			ep_buf[0].out_len);
	}
	else {
		/* A packet received as part of the data stage of
		 * a control transfer. Pack what data we received
		 * into the application's buffer (if it has
		 * provided one). When all the data has been
		 * received, call the application-provided callback.
		 */

		if (ep0_data_stage_out_buffer) {
			uint8_t bytes_to_copy = MIN(pkt_len, ep0_data_stage_buf_remaining);
			memcpy(ep0_data_stage_out_buffer, ep_buf[0].out, bytes_to_copy);
			ep0_data_stage_out_buffer += bytes_to_copy;
			ep0_data_stage_buf_remaining -= bytes_to_copy;

			/* It's possible that bytes_to_copy is less than pkt_len
			 * here because the application provided too small a buffer. */

			if (pkt_len < EP_0_OUT_LEN || ep0_data_stage_buf_remaining == 0) {
				/* Short packet or we've received the expected length.
				 * All data has been transferred (or all the data
				 * has been received which can be received). */

				if (bytes_to_copy < pkt_len) {
					/* The buffer provided by the application was too short */
					stall_ep0();
					ep0_data_stage_callback(0/*false*/, ep0_data_stage_context);
					reset_ep0_data_stage();
				}
				else {
					/* The data stage has completed. Set up the status stage. */
					send_zero_length_packet_ep0();
				}
			}
		}
	}
}

static inline void handle_ep0_in()
{
	if (addr_pending) {
		SFR_USB_ADDR =  addr;
		addr_pending = 0;
	}

	if (ep0_data_stage_buf_remaining) {
		/* There's already a multi-transaction transfer in process. */
		uint8_t bytes_to_send = MIN(ep0_data_stage_buf_remaining, EP_0_IN_LEN);

		memcpy_from_rom(ep_buf[0].in, ep0_data_stage_in_buffer, bytes_to_send);
		ep0_data_stage_buf_remaining -= bytes_to_send;
		ep0_data_stage_in_buffer += bytes_to_send;

		/* If we hit the end with a full-length packet, set up
		   to send a zero-length packet at the next IN token, but only
		   if we are returning less data than was requested. */
		if (ep0_data_stage_buf_remaining == 0 &&
		    bytes_to_send == EP_0_IN_LEN &&
		    returning_short)
			control_need_zlp = 1;

		usb_send_in_buffer(0, bytes_to_send);
	}
	else if (control_need_zlp) {
		usb_send_in_buffer(0, 0);
		control_need_zlp = 0;
		reset_ep0_data_stage();
	}
	else {
		if (ep0_data_stage_direc == 0/*OUT*/) {
			/* An IN on the control endpoint with no data pending
			 * and during an OUT transfer means the STATUS stage
			 * of the control transfer has completed. Notify the
			 * application, if applicable. */
			if (ep0_data_stage_callback)
				ep0_data_stage_callback(1/*true*/, ep0_data_stage_context);
			reset_ep0_data_stage();
		}
	}
}

/* checkUSB() is called repeatedly to check for USB interrupts
   and service USB requests */
void usb_service(void)
{
	if (SFR_USB_RESET_IF) {
		/* A Reset was detected on the wire. Re-init the SIE. */
#ifdef USB_RESET_CALLBACK
		USB_RESET_CALLBACK();
#endif
		usb_init();
		CLEAR_USB_RESET_IF();
		SERIAL("USB Reset");
	}
	
	if (SFR_USB_STALL_IF) {
		CLEAR_USB_STALL_IF();
	}


	if (SFR_USB_TOKEN_IF) {

		//struct ustat_bits ustat = *((struct ustat_bits*)&USTAT);

		if (SFR_USB_STATUS_EP == 0 && SFR_USB_STATUS_DIR == 0/*OUT*/) {
			/* An OUT or SETUP transaction has completed on
			 * Endpoint 0.  Handle the data that was received.
			 */
			if (bds[0].ep_out.STAT.PID == PID_SETUP) {
				handle_ep0_setup();
			}
			else if (bds[0].ep_out.STAT.PID == PID_IN) {
				/* Nonsense condition:
				   (PID IN on SFR_USB_STATUS_DIR == OUT) */
			}
			else if (bds[0].ep_out.STAT.PID == PID_OUT) {
				handle_ep0_out();
			}
			else {
				/* Unsupported PID. Stall the Endpoint. */
				SERIAL("Unsupported PID. Stall.");
				stall_ep0();
			}

			reset_bd0_out();
		}
		else if (SFR_USB_STATUS_EP == 0 && SFR_USB_STATUS_DIR == 1/*1=IN*/) {
			/* An IN transaction has completed. The endpoint
			 * needs to be re-loaded with the next transaction's
			 * data if there is any.
			 */
			handle_ep0_in();
		}
		else if (SFR_USB_STATUS_EP > 0 && SFR_USB_STATUS_EP <= NUM_ENDPOINT_NUMBERS) {
			if (SFR_USB_STATUS_DIR == 1 /*1=IN*/) {
				/* An IN transaction has completed. */
				SERIAL("IN transaction completed on non-EP0.");
				if (ep_buf[SFR_USB_STATUS_EP].flags & EP_IN_HALT_FLAG)
					stall_ep_in(SFR_USB_STATUS_EP);
				else {

				}
			}
			else {
				/* An OUT transaction has completed. */
				SERIAL("OUT transaction received on non-EP0");
				if (ep_buf[SFR_USB_STATUS_EP].flags & EP_OUT_HALT_FLAG)
					stall_ep_out(SFR_USB_STATUS_EP);
				else {

				}
			}
		}
		else {
			/* Transaction completed on an endpoint not used.
			 * This should never happen. */
			SERIAL("Transaction completed for unknown endpoint");
		}

		CLEAR_USB_TOKEN_IF();
	}
	
	/* Check for Start-of-Frame interrupt. */
	if (SFR_USB_SOF_IF) {
#ifdef START_OF_FRAME_CALLBACK
		START_OF_FRAME_CALLBACK();
#endif
		CLEAR_USB_SOF_IF();
	}

	/* Check for USB Interrupt. */
	if (SFR_USB_IF) {
		SFR_USB_IF = 0;
	}
}

uint8_t usb_get_configuration(void)
{
	return g_configuration;
}

unsigned char *usb_get_in_buffer(uint8_t endpoint)
{
	return ep_buf[endpoint].in;
}

void usb_send_in_buffer(uint8_t endpoint, size_t len)
{
	if ((g_configuration > 0 || endpoint == 0) && !usb_in_endpoint_halted(endpoint)) {
		uint8_t pid;
		pid = !bds[endpoint].ep_in.STAT.DTS;
		bds[endpoint].ep_in.STAT.BDnSTAT = 0;

		if (pid)
			SET_BDN(bds[endpoint].ep_in,
				BDNSTAT_UOWN|BDNSTAT_DTS|BDNSTAT_DTSEN, len);
		else
			SET_BDN(bds[endpoint].ep_in,
				BDNSTAT_UOWN|BDNSTAT_DTSEN, len);
	}
}

bool usb_in_endpoint_busy(uint8_t endpoint)
{
	return bds[endpoint].ep_in.STAT.UOWN;
}

bool usb_in_endpoint_halted(uint8_t endpoint)
{
	return ep_buf[endpoint].flags & EP_IN_HALT_FLAG;
}

uint8_t usb_get_out_buffer(uint8_t endpoint, const unsigned char **buf)
{
	*buf = ep_buf[endpoint].out;
	return BDN_LENGTH(bds[endpoint].ep_out);
}

bool usb_out_endpoint_has_data(uint8_t endpoint)
{
	return !bds[endpoint].ep_out.STAT.UOWN;
}

void usb_arm_out_endpoint(uint8_t endpoint)
{
	uint8_t pid = !bds[endpoint].ep_out.STAT.DTS;
	if (pid)
		SET_BDN(bds[endpoint].ep_out,
			BDNSTAT_UOWN|BDNSTAT_DTS|BDNSTAT_DTSEN,
			ep_buf[endpoint].out_len);
	else
		SET_BDN(bds[endpoint].ep_out,
			BDNSTAT_UOWN|BDNSTAT_DTSEN,
			ep_buf[endpoint].out_len);
}

bool usb_out_endpoint_halted(uint8_t endpoint)
{
	return ep_buf[endpoint].flags & EP_OUT_HALT_FLAG;
}

void usb_start_receive_ep0_data_stage(char *buffer, size_t len,
                                      usb_ep0_data_stage_callback callback, void *context)
{
	reset_ep0_data_stage();

	ep0_data_stage_callback = callback;
	ep0_data_stage_out_buffer = buffer;
	ep0_data_stage_buf_remaining = len;
	ep0_data_stage_context = context;
}

void usb_send_data_stage(char *buffer, size_t len,
	usb_ep0_data_stage_callback callback, void *context)
{
	/* Start sending the first block. Subsequent blocks will be sent
	   when IN tokens are received on endpoint zero. */
	start_control_return(buffer, len, len);

	ep0_data_stage_callback = callback;
	ep0_data_stage_context = context;
}



#ifdef USB_USE_INTERRUPTS
#ifdef __XC16__

void _ISR __attribute((auto_psv)) _USB1Interrupt()
{
	usb_service();
}

#elif __C18
#elif __XC8
	/* On these systems, interupt handlers are shared. An interrupt
	 * handler from the application must call usb_service(). */
#else
#error Compiler not supported yet
#endif
#endif