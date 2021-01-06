/*
    This file is part of SX126x Linux driver.
    Copyright (C) 2020, 2021 ReimuNotMoe

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "SX126x_Linux.hpp"

#include <ctime>


timespec timespec_sub(timespec bigger, timespec smaller) {
	timespec ret;
	ret.tv_sec = bigger.tv_sec - smaller.tv_sec;
	ret.tv_nsec = bigger.tv_nsec - smaller.tv_nsec;

	if (ret.tv_nsec < 0) {
		ret.tv_sec--;
		ret.tv_nsec += 1e9;
	}

	return ret;
}



int main(int argc, char **argv) {
	if (argc < 8) {
		printf("Usage: SX126x_Test <txpower> <freq in kHz> <bw 6..4> <cr 1..4> <sf 5..12> <lna_enable> <tx|rx>\n"
		       "e.g. SX126x_Test 22 433000 6 1 5 1 tx\n");
		return 1;
	}

	int16_t rxen_pin = 20;

	if (strtol(argv[6], nullptr, 10) == 0) {
		rxen_pin = -1;
	}

	// Customize these pins by yourself
	SX126x_Linux Radio("/dev/spidev0.0", 0,
			   {
				   19, 13, 23,
				   26, -1, -1,
				   21, rxen_pin
			   }
	);

	// Assume we're running on a high-end Raspberry Pi,
	// so we set the SPI clock speed to the maximum value supported by the chip
	Radio.SetSpiSpeed(8000000);

	// WARNING: Change this to your chip type, or it will misconfigure RF PA
	Radio.SetDeviceType(SX126x::SX1268);

	Radio.Init();
	puts("Init done");

	// WARNING: Change this to reflect your configuration
	// Or you may burn the chips
	Radio.SetDio3AsTcxoCtrl(SX126x::TCXO_CTRL_1_8V, 32);
	puts("SetDio3AsTcxoCtrl done");

	SX126x::CalibrationParams_t calib_param;
	calib_param.Value = 0x7f;
	Radio.Calibrate(calib_param);
	puts("Calibrate done");

	Radio.SetStandby(SX126x::STDBY_XOSC);
	puts("SetStandby done");

	auto txpwr = strtol(argv[1], nullptr, 10);
	Radio.SetTxParams(txpwr, SX126x::RADIO_RAMP_10_US);
	puts("SetTxParams done");

	Radio.SetBufferBaseAddresses(0x00, 0x00);
	puts("SetBufferBaseAddresses done");

	SX126x::ModulationParams_t ModulationParams2;
	ModulationParams2.PacketType = SX126x::PACKET_TYPE_LORA;
	ModulationParams2.Params.LoRa.CodingRate = static_cast<SX126x::RadioLoRaCodingRates_t>(strtol(argv[4], nullptr, 10));
	ModulationParams2.Params.LoRa.Bandwidth = static_cast<SX126x::RadioLoRaBandwidths_t>(strtol(argv[3], nullptr, 10));
	ModulationParams2.Params.LoRa.SpreadingFactor = static_cast<SX126x::RadioLoRaSpreadingFactors_t>(strtol(argv[5], nullptr, 10));
	ModulationParams2.Params.LoRa.LowDatarateOptimize = 0;

	SX126x::PacketParams_t PacketParams2;
	PacketParams2.PacketType = SX126x::PACKET_TYPE_LORA;
	auto &l = PacketParams2.Params.LoRa;
	l.PayloadLength = 32;
	l.HeaderType = SX126x::LORA_PACKET_VARIABLE_LENGTH;
	l.PreambleLength = 24;
	l.CrcMode = SX126x::LORA_CRC_ON;
	l.InvertIQ = SX126x::LORA_IQ_NORMAL;


	Radio.SetPacketType(SX126x::PACKET_TYPE_LORA);
	puts("SetPacketType done");
	Radio.SetModulationParams(ModulationParams2);
	puts("SetModulationParams done");
	Radio.SetPacketParams(PacketParams2);
	puts("SetPacketParams done");

	auto freq = strtol(argv[2], nullptr, 10);
	Radio.SetRfFrequency(freq * 1000UL);
	puts("SetRfFrequency done");

	Radio.callbacks.txDone = []{
		puts("Wow TX done");
	};

	size_t pkt_count = 0;

	timespec tm_curpkt, tm_lastpkt;
	clock_gettime(CLOCK_MONOTONIC, &tm_lastpkt);

	Radio.callbacks.rxDone = [&] {
		clock_gettime(CLOCK_MONOTONIC, &tm_curpkt);
		timespec tm_diff = timespec_sub(tm_curpkt, tm_lastpkt);
		tm_lastpkt = tm_curpkt;

		puts("Wow RX done");

		SX126x::PacketStatus_t ps;
		Radio.GetPacketStatus(&ps);

		uint8_t recv_buf[253];
		uint8_t rsz;
		Radio.GetPayload(recv_buf, &rsz, 253);

		uint8_t err_count = 0;

		for (size_t i=0; i<rsz; i++) {
			uint8_t correct_value;
			if (i % 2)
				correct_value = 0x55;
			else
				correct_value = 0xaa;

			if (recv_buf[i] != correct_value)
				err_count++;
		}

		const char *color, *color2;

		double timediff = (double)tm_diff.tv_sec + ((double)tm_diff.tv_nsec / 1e9);
		double airtime = (double)Radio.GetTimeOnAir() / 1000 + 0.02;
		double bounce_offset_ms = (timediff - airtime) * 1000;

		if (timediff > (airtime + 0.008))
			color = "\x1b[01;31m";
		else
			color = "\x1b[01;32m";

		if (bounce_offset_ms > 3)
			color2 = "\x1b[01;31m";
		else
			color2 = "\x1b[01;32m";


		pkt_count++;

		time_t tm = time(nullptr);
		auto tmtm = localtime(&tm);

		char timebuf[64];

		strftime(timebuf, 63, "%Y-%m-%d %H:%M:%S", tmtm);
		puts(timebuf);

		printf("pkt count: %ld, %slast packet: %lfs/%lfs ago, %sbounce offset: %lfms\x1b[0m\n", pkt_count, color, timediff, airtime, color2, bounce_offset_ms);

		double ber = (double)err_count/rsz*100;
		if (ber == 0.0)
			color = "\x1b[01;32m";
		else if (ber <= 5.0)
			color = "\x1b[01;33m";
		else
			color = "\x1b[01;31m";


		printf("corrupted bytes: %u/%u, %sBER: %f%%\x1b[0m\n", err_count, rsz, color, ber);

		auto snr = ps.Params.LoRa.SnrPkt;
		if (snr >= 3)
			color = "\x1b[01;32m";
		else if (snr < 3 && snr >= -3)
			color = "\x1b[01;33m";
		else
			color = "\x1b[01;31m";


		printf("recvd %u bytes, RSCP: %d, RSSI: %d, %sSNR: %d\x1b[0m\n", rsz, ps.Params.LoRa.SignalRssiPkt, ps.Params.LoRa.RssiPkt, color, ps.Params.LoRa.SnrPkt);
	};


	auto IrqMask = SX126x::IRQ_RX_DONE | SX126x::IRQ_TX_DONE | SX126x::IRQ_RX_TX_TIMEOUT;
	Radio.SetDioIrqParams(IrqMask, IrqMask, SX126x::IRQ_RADIO_NONE, SX126x::IRQ_RADIO_NONE);
	puts("SetDioIrqParams done");

	Radio.StartIrqHandler();
	puts("StartIrqHandler done");

//	Radio.SetTxInfinitePreamble();
//
//	while (1) {
//		sleep(1);
//	}


	auto pkt_ToA = Radio.GetTimeOnAir();

	if (strcmp(argv[7], "tx") == 0) {
		uint8_t buf[253];

		for (size_t i=0; i<sizeof(buf); i++) {
			if (i % 2)
				buf[i] = 0x55;
			else
				buf[i] = 0xaa;
		}

		while (1) {
			Radio.SendPayload((uint8_t *) buf, 32, 0);

			puts("SendPayload done");
			usleep((pkt_ToA + 20) * 1000);
		}
	} else {
		Radio.SetRxBoosted(UINT32_MAX);

		while (1) {
			sleep(1);
		}
	}

}
