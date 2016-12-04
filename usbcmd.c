#include <stdio.h>
#include <libusb.h>

int main(int argc, char *argv[])
{
	libusb_context *context = NULL;
	libusb_device_handle *handle = NULL;
	uint16_t pid, vid;
	int ret;
	if (argc < 2) {
		return -1;
	}
	sscanf(argv[1], "%hx:%hx", &vid, &pid);
	ret = libusb_init(&context);
	if (0 != ret) {
		printf("init failed %d\n", ret);
		return -1;
	}
	printf("0x%hx:0x%hx\n", vid, pid);
	handle = libusb_open_device_with_vid_pid(context, vid, pid);
	if (handle) {
		printf("open successfully\n");
		int ret = libusb_control_transfer(handle, 0x40, 0xf0, 0x0201, 0, NULL, 0, 0);
		if (LIBUSB_ERROR_PIPE == ret) {
			printf("not supported\n");
		} else if (0 == ret) {
			printf("mirrorlink command OK\n");
		}
		else {
			printf("transfer error\n");
		}
	}
	libusb_close(handle);
	libusb_exit(context);
	return 0;
}
