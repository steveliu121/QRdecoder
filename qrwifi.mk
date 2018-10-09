BUILD_DIR=$(shell pwd)/build

all: prepare zxing

prepare:
	mkdir -p $(BUILD_DIR)

zxing:
	cd $(BUILD_DIR); \
	cmake .. \
		-DCMAKE_INSTALL_PREFIX=$(BUILD_DIR) \
		-DCMAKE_BUILD_TYPE=Debug; \
	make; \
	make install; \
	mv ./zxing qrwifi; \
	mv ./bin/zxing ./bin/qrwifi

clean:
	rm -rf $(BUILD_DIR)
