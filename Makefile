SOURCE_FILES:= \
	main.c \
	utils.c \
	conn.c \
	dapclient.c \
	sigverify.c

PKG_CONFIG_LIBS:= \
	libevent \
	libxml-2.0 \
	libcrypto

all: $(SOURCE_FILES)
	gcc $(SOURCE_FILES) `pkg-config --cflags --libs $(PKG_CONFIG_LIBS)` -o main

clean:
	rm -rf *.o
	rm main
