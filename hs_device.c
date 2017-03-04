#include <libusb.h>

struct hs_device
{
	libusb_context *ctx;
	libusb_hotplug_callback_handle cb_handle;
	uint8_t bulk;
	int ifnum;
	libusb_device_handle *dev_handle;
	int busy;
};

static struct hs_device dev;

static int libusb_hotplug_cb(libusb_context *ctx, libusb_device *device, libusb_hotplug_event event, void *user_data);

int hs_device_init()
{
	int ret;
	libusb_context *ctx;
	ret = libusb_init(&ctx);
	if (ret) {
		return -1;
	}
	dev.ctx = ctx;
	libusb_hotplug_register_callback(ctx, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT,
		LIBUSB_HOTPLUG_ENUMERATE, LIBUSB_HOTPLUG_MATCH_ANY, LIBUSB_HOTPLUG_MATCH_ANY,
		LIBUSB_CLASS_VENDOR_SPEC, libusb_hotplug_cb, NULL, &(dev.cb_handle));
}

int libusb_hotplug_callback_fn(libusb_context *ctx, libusb_device *device, libusb_hotplug_event event, void *user_data)
{
	switch (event) {
		case LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED:
		{
			struct libusb_device_descriptor desc;
			struct libusb_config_descriptor *config = NULL;
			uint8_t i, j, k;
			struct hs_device *hsd = (struct hs_device *)user_data;
			if (hsd->busy) {
				return 0;
			}
			libusb_get_device_descriptor(device, &desc);
			if (0xCC != desc.bDeviceSubClass) {
				return 0;
			}
			if (0x01 != desc.bDeviceProtocol) {
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
					if (0xCC != ifdesc->bInterfaceSubClass) {
						continue;
					}
					if (0x01 != ifdesc->bInterfaceProtocol) {
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
						if (((endpoint->bmAttributes & 0x3) == LIBUSB_TRANSFER_TYPE_BULK)
							&& ((endpoint->bEndpointAddress & 0x7) == LIBUSB_ENDPOINT_IN)) {
							m++;
							n = bEndpointAddress;
						}
					}
					if (2 == m) {
						libusb_config_descriptor *active = NULL;
						if (libusb_open(device, &(hsd->dev_handle))) {
							continue;
						}
						if (!libusb_get_active_config_descriptor(device, &active)) {
							if (active->bConfigurationValue != config->bConfigurationValue) {
								libusb_set_configuration(hsd->dev_handle, config->bConfigurationValue);
							}
						}
						if (!libusb_claim_interface(hsd->dev_handle, ifdesc->bInterfaceNumber)) {
							hsd->ifnum = ifdesc->bInterfaceNumber;
						}
						hsd->bulk = n;
						hsd->busy = 1;
					}
				}
			}
			libusb_free_config_descriptor(config);
		}
		break;
	}
}