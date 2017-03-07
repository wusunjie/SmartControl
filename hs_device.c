#include <libusb.h>
#include <string.h>
#include "list.h"


#define HSML_SET_CMD							0x41
#define HSML_GET_CMD							0xC1

#define HSML_GET_VERSION_REQ					0x40
#define HSML_GET_PARAMETERS_REQ					0x41
#define HSML_SET_PARAMETERS_REQ					0x42
#define HSML_START_FRAMEBUFFER_TRANSMISSION_REQ 0x43
#define HSML_PAUSE_FRAMEBUFFER_TRANSMISSION_REQ 0x44
#define HSML_STOP_FRAMEBUFFER_TRANSMISSION_REQ  0x45
#define HSML_SET_MAX_FRAME_RATE_REQ				0x46
#define HSML_GET_IDENTIFIER_REQ					0x47

#define HSML_START_FRAMEBUFFER_STREAMING_MODE   0x0000
#define HSML_START_FRAMEBUFFER_ONDEMAND_MODE    0x0001

#define HSML_SUBCLASS_CODE						0xCC
#define HSML_PROTOCOL_CODE						0x01

struct hs_device
{
	struct libusb_device *device;
	uint8_t baddr;
	int ifnum;
	libusb_device_handle *dev_handle;
	libusb_transfer *control;
	libusb_transfer *bulk;
	struct list_head list;
};

static libusb_context *libusb_ctx = NULL;
static libusb_hotplug_callback_handle libusb_hotplug_handle;
static struct hs_device *active_dev = NULL;
static struct list_head hs_device_list;
static const char *hs_device_uuid = NULL;

static int libusb_hotplug_cb(libusb_context *ctx, libusb_device *device, libusb_hotplug_event event, void *user_data);
static void libusb_transfer_control_cb(struct libusb_transfer *transfer);
static void libusb_transfer_bulk_cb(struct libusb_transfer *transfer);
static struct hs_device *hs_device_create(struct libusb_device *device, int ifnum, uint8_t baddr);
static int hs_device_open(struct hs_device *dev);
static int hs_device_get_identifier(struct hs_device *dev);

int hs_device_init()
{
	int ret;
	ret = libusb_init(&libusb_ctx);
	if (ret) {
		return -1;
	}
	INIT_LIST_HEAD(&hs_device_list);
	hs_device_uuid = NULL;
	active_dev = NULL;
	return libusb_hotplug_register_callback(libusb_ctx, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT,
		LIBUSB_HOTPLUG_ENUMERATE, LIBUSB_HOTPLUG_MATCH_ANY, LIBUSB_HOTPLUG_MATCH_ANY,
		LIBUSB_CLASS_VENDOR_SPEC, libusb_hotplug_cb, NULL, &libusb_hotplug_handle);
}

int hs_device_start(const char *uuid)
{
	struct hs_device *dev;
	struct hs_device *tmp;
	if (hs_device_uuid) {
		free(hs_device_uuid);
	}
	hs_device_uuid = strdup(uuid);
	list_for_each_entry(dev, tmp, &hs_device_list, list) {
		if (hs_device_open(dev)) {
			list_del(&(dev->list));
			hs_device_destory(dev);
		}
		else if (hs_device_get_identifier(dev)) {
			list_del(&(dev->list));
			hs_device_destory(dev);
		}
	}
}

int libusb_hotplug_cb(libusb_context *ctx, libusb_device *device, libusb_hotplug_event event, void *user_data)
{
	switch (event) {
		case LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED:
		{
			struct libusb_device_descriptor desc;
			struct libusb_config_descriptor *config = NULL;
			uint8_t i, j, k;
			if (active_dev) {
				return 0;
			}
			libusb_get_device_descriptor(device, &desc);
			if (HSML_SUBCLASS_CODE != desc.bDeviceSubClass) {
				return 0;
			}
			if (HSML_PROTOCOL_CODE != desc.bDeviceProtocol) {
				return 0;
			}
			if (libusb_get_config_descriptor(device, &config)) {
				return 0;
			}
			for (i = 0; i < config->bNumInterfaces; i++) {
				for (j = 0; j < config->interface[i].num_altsetting; j++) {
					const struct libusb_interface_descriptor *ifdesc = config->interface[i].altsetting[j];
					int m = 0;
					uint8_t n = 0;
					if (LIBUSB_CLASS_VENDOR_SPEC != ifdesc->bInterfaceClass) {
						continue;
					}
					if (HSML_SUBCLASS_CODE != ifdesc->bInterfaceSubClass) {
						continue;
					}
					if (HSML_PROTOCOL_CODE != ifdesc->bInterfaceProtocol) {
						continue;
					}
					if (2 > ifdesc->bNumEndpoints) {
						continue;
					}
					for (k = 0; k < ifdesc->bNumEndpoints; k++) {
						const struct libusb_endpoint_descriptor *endpoint = ifdesc->endpoint[k];
						if ((endpoint->bmAttributes & 0x3) == LIBUSB_TRANSFER_TYPE_CONTROL) {
							m++;
						}
						else if (((endpoint->bmAttributes & 0x3) == LIBUSB_TRANSFER_TYPE_BULK)
							&& ((endpoint->bEndpointAddress & 0x7) == LIBUSB_ENDPOINT_IN)) {
							m++;
							n = bEndpointAddress;
						}
					}
					if (2 == m) {
						struct hs_device *dev = hs_device_create(device, ifdesc->bInterfaceNumber, n);
						if (hs_device_uuid) {
							if (hs_device_open(dev)) {
								hs_device_destory(dev);
								continue;
							}
						}
						list_add(&hs_device_list, &(dev->list));
					}
				}
			}
			libusb_free_config_descriptor(config);
		}
		break;
	}
}

void libusb_transfer_control_cb(struct libusb_transfer *transfer)
{
	struct hs_device *dev = (struct hs_device *)transfer->user_data;
	switch (transfer->status) {
		case LIBUSB_TRANSFER_CANCELLED:
		{
			if (!dev->bulk) {
				list_del(&(dev->list));
				libusb_release_interface(dev->device, dev->ifnum);
				libusb_free_transfer(dev->control);
				libusb_close(dev->dev_handle);
				libusb_unref_device(dev->device);
				free(dev);
			}
			else {
				libusb_free_transfer(dev->control);
				dev->control = NULL;
			}
		}
		break;
		case LIBUSB_TRANSFER_COMPLETED:
		case LIBUSB_TRANSFER_OVERFLOW:
		{
			if (transfer->actual_length >= LIBUSB_CONTROL_SETUP_SIZE) {
				struct libusb_control_setup *setup = (struct libusb_control_setup *)transfer->buffer;
				if (transfer->actual_length >= LIBUSB_CONTROL_SETUP_SIZE + libusb_le16_to_cpu(setup->wLength)) {
					switch (setup->bRequest) {
					case HSML_GET_IDENTIFIER_REQ:
					{

					}
					break;
					default:
					break;
					}
				}
			}
		}
		break;
		default:
		break;
	}
}

void libusb_transfer_bulk_cb(struct libusb_transfer *transfer)
{
	struct hs_device *dev = (struct hs_device *)transfer->user_data;
	switch (transfer->status) {
		case LIBUSB_TRANSFER_CANCELLED:
		{
			if (!dev->control) {
				list_del(&(dev->list));
				libusb_release_interface(dev->device, dev->ifnum);
				libusb_free_transfer(dev->bulk);
				libusb_close(dev->dev_handle);
				libusb_unref_device(dev->device);
				free(dev);
			}
			else {
				libusb_free_transfer(dev->bulk);
				dev->bulk = NULL;
			}
		}
		break;
		default:
		break;
	}
}

struct hs_device *hs_device_create(struct libusb_device *device, int ifnum, uint8_t baddr)
{
	struct hs_device *hsd = (struct hs_device *)malloc(sizeof(*hsd));
	hsd->ifnum = ifnum;
	hsd->baddr = baddr;
	hsd->device = libusb_ref_device(device);
	return hsd;
}

void hs_device_destory(struct hs_device *dev)
{
	if (dev) {
		if (libusb_cancel_transfer(dev->control) && libusb_cancel_transfer(dev->bulk)) {
			libusb_release_interface(dev->device, dev->ifnum);
			libusb_free_transfer(dev->control);
			libusb_free_transfer(dev->bulk);
			libusb_close(dev->dev_handle);
			libusb_unref_device(dev->device);
			free(dev);
		}
	}
}

int hs_device_open(struct hs_device *dev)
{
	if (libusb_open(dev->device, &(dev->dev_handle))) {
		return -1;
	}
	if (libusb_claim_interface(dev->dev_handle, dev->ifnum)) {
		libusb_close(dev->dev_handle);
		return -1;
	}
	dev->control = libusb_alloc_transfer(0);
	dev->bulk = libusb_alloc_transfer(0);
	dev->control->flags = LIBUSB_TRANSFER_ADD_ZERO_PACKET | LIBUSB_TRANSFER_FREE_BUFFER;
	dev->bulk->flags = LIBUSB_TRANSFER_ADD_ZERO_PACKET | LIBUSB_TRANSFER_FREE_BUFFER;
	return 0;
}

int hs_device_get_identifier(struct hs_device *dev)
{
	unsigned char *buffer = (unsigned char *)malloc(LIBUSB_CONTROL_SETUP_SIZE + 16);
	libusb_fill_control_setup(buffer, HSML_GET_CMD, HSML_GET_IDENTIFIER_REQ, 0, dev->ifnum, 16);
	libusb_fill_control_transfer(dev->control, dev->dev_handle, buffer,
		libusb_transfer_control_cb, dev, 0);
	return libusb_submit_transfer(dev->control);
}