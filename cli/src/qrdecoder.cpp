// -*- mode:c++; tab-width:2; indent-tabs-mode:nil; c-basic-offset:2 -*-
/*
 *  Copyright 2010-2011 ZXing authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <iostream>
#include <fstream>
#include <string>
#include "ImageReaderSource.h"
#include <zxing/common/Counted.h>
#include <zxing/Binarizer.h>
#include <zxing/MultiFormatReader.h>
#include <zxing/Result.h>
#include <zxing/ReaderException.h>
#include <zxing/common/GlobalHistogramBinarizer.h>
#include <zxing/common/HybridBinarizer.h>
#include <exception>
#include <zxing/Exception.h>
#include <zxing/common/IllegalArgumentException.h>
#include <zxing/BinaryBitmap.h>
#include <zxing/DecodeHints.h>

#include <zxing/qrcode/QRCodeReader.h>
#include <zxing/multi/qrcode/QRCodeMultiReader.h>
#include <zxing/multi/ByQuadrantReader.h>
#include <zxing/multi/MultipleBarcodeReader.h>
#include <zxing/multi/GenericMultipleBarcodeReader.h>

using namespace std;
using namespace zxing;
using namespace zxing::multi;
using namespace zxing::qrcode;

namespace {
	bool try_harder = true;
	bool search_multi = false;
}

vector<Ref<Result> > decode(Ref<BinaryBitmap> image, DecodeHints hints) {
  Ref<Reader> reader(new MultiFormatReader);
  return vector<Ref<Result> >(1, reader->decode(image, hints));
}

vector<Ref<Result> > decode_multi(Ref<BinaryBitmap> image, DecodeHints hints) {
  MultiFormatReader delegate;
  GenericMultipleBarcodeReader reader(delegate);
  return reader.decodeMultiple(image, hints);
}

int read_image(Ref<LuminanceSource> source,
			bool hybrid, vector<string>& ret_str)
{
	vector<Ref<Result> > results;
	string cell_result;
	int res = -1;

	try {
		 Ref<Binarizer> binarizer;

		if (hybrid)
			binarizer = new HybridBinarizer(source);
		else
			binarizer = new GlobalHistogramBinarizer(source);

		DecodeHints hints(DecodeHints::DEFAULT_HINT);
		hints.setTryHarder(try_harder);
		Ref<BinaryBitmap> binary(new BinaryBitmap(binarizer));
		if (search_multi)
			results = decode_multi(binary, hints);
		else
			results = decode(binary, hints);

		res = 0;

	} catch (const ReaderException& e) {
		cell_result = "zxing::ReaderException: " + string(e.what());
		res = -2;
	} catch (const zxing::IllegalArgumentException& e) {
		cell_result = "zxing::IllegalArgumentException: " + string(e.what());
		res = -3;
	} catch (const zxing::Exception& e) {
		cell_result = "zxing::Exception: " + string(e.what());
		res = -4;
	} catch (const std::exception& e) {
		cell_result = "std::exception: " + string(e.what());
		res = -5;
	}

	if (res) {
		cout << cell_result << "::res" << res << endl;
		return res;
	}

	for (size_t i = 0; i < results.size(); i++) {
		cout << "  Format: "
			<< BarcodeFormat::barcodeFormatNames[results[i]->getBarcodeFormat()]
			<< endl;
		for (int j = 0; j < results[i]->getResultPoints()->size(); j++) {
			cout << "  Point[" << j <<  "]: "
				<< results[i]->getResultPoints()[j]->getX() << " "
				<< results[i]->getResultPoints()[j]->getY() << endl;
		}

		cout << results[i]->getText()->getText() << endl;
		ret_str.push_back(results[i]->getText()->getText());
	}

	return 0;
}

#define OPEN 0
#define WPA 1
#define WPA_CONF_FILE "/etc/wpa_supplicant/wpa_supplicant.conf"

int __connect_wifi(vector<string>& src_str)
{
	string ssid;
	string passwd;
	vector<string> wifi_str;
	size_t pos_head = 0;
	size_t pos_tail = 0;
	int len;
	int auth_type;

	if (src_str.empty()) {
		cout << "QR msg empty!" << endl;
		return -1;
	}

	/* split source QR decoded string */
	while ((pos_tail = src_str[0].find(";", pos_head)) != string::npos) {
		len = pos_tail - pos_head + 1;
		wifi_str.push_back(src_str[0].substr(pos_head, len));
		pos_head = pos_tail + 1;
	}

	/* check if wifi msg string */
	if (wifi_str[0].find("WIFI:T:") == string::npos) {
		cout << "Not a WIFI msg!" << endl;
		return -1;
	}

	/* check authentication type */
	if (wifi_str[0].find("nopass") != string::npos)
		auth_type = OPEN;
	else if (wifi_str[0].find("WPA") != string::npos)
		auth_type = WPA;
	else {
		cout << "Could not recognize auth type!" << endl;
		return -1;
	}

	/* get ssid */
	pos_head = wifi_str[1].find(":");
	pos_tail = wifi_str[1].find(";");
	len = pos_tail - pos_head + 1;
	ssid = wifi_str[1].substr(pos_head + 1, len - 2);

	/* get password */
	if (auth_type != OPEN) {
		pos_head = wifi_str[2].find(":");
		pos_tail = wifi_str[2].find(";");
		len = pos_tail - pos_head + 1;
		passwd = wifi_str[2].substr(pos_head + 1, len - 2);
	}

	/* write wpa_supplicant configure file */
	ofstream wpa_conf_file(WPA_CONF_FILE);

	wpa_conf_file << "ctrl_interface=DIR=/var/run/wpa_supplicant GROUP=netdev" << endl
	<< "update_config=1" << endl
	<< "country=CN" << endl
	<< endl
	<< "network={" << endl
	<< "\tssid=\"" << ssid << "\"" << endl;

	if (auth_type == OPEN)
		wpa_conf_file << "\tkey_mgmt=NONE" << endl;
	if (auth_type == WPA)
		wpa_conf_file << "\tpsk=\"" << passwd << "\"" << endl;

	wpa_conf_file << "}" << endl;

	wpa_conf_file.close();

	system("wpa_supplicant -B -c/etc/wpa_supplicant/wpa_supplicant.conf -iwlan0 -Dnl80211,wext -P /var/run/wpa_supplicant.pid");

	return 0;

}

int main(int argc, char** argv)
{
	int gresult = 1;
	int hresult = 1;
	vector<string> ret_str;
	string filename;

	for (int i = 1; i < argc; i++) {
		filename = argv[i];
		if (filename.compare("--search_multi") == 0 ||
			filename.compare("-s") == 0)
			search_multi = true;
	}

	Ref<LuminanceSource> source;
	try {
		source = ImageReaderSource::create(filename);
	} catch (const zxing::IllegalArgumentException &e) {
		cerr << e.what() << " (ignoring)" << endl;
		return -1;
	}

	/* use HybridBinarizer by default */
	hresult = read_image(source, true, ret_str);
	if (hresult == 0)
		__connect_wifi(ret_str);
	else {
		/* use GlobalHistogramBinarizer as replacement */
		gresult = read_image(source, false, ret_str);
		if (gresult == 0)
			__connect_wifi(ret_str);
		else
			cout << "decoding failed" << endl;
	}

	return 0;
}
