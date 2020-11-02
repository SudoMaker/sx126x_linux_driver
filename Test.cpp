/*
    This file is part of SX126x Linux driver.
    Copyright (C) 2020 ReimuNotMoe

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "SX126x_Linux.hpp"

int main() {
	// Customize these pins by yourself
	SX126x_Linux Radio("/dev/spidev0.0", 0,
			   {
				   19, 13, 23,
				   26, -1, -1,
				   21, 20
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
	Radio.SetDio3AsTcxoCtrl(SX126x::TCXO_CTRL_1_8V, 320);
	puts("SetDio3AsTcxoCtrl done");

	SX126x::CalibrationParams_t calib_param;
	calib_param.Value = 0x7f;
	Radio.Calibrate(calib_param);
	puts("Calibrate done");

	Radio.SetStandby(SX126x::STDBY_XOSC);
	puts("SetStandby done");

	Radio.SetTxParams(22, SX126x::RADIO_RAMP_200_US);
	puts("SetTxParams done");

	Radio.SetBufferBaseAddresses(0x00, 0x00);
	puts("SetBufferBaseAddresses done");

	SX126x::ModulationParams_t ModulationParams2;
	ModulationParams2.PacketType = SX126x::PACKET_TYPE_LORA;
	ModulationParams2.Params.LoRa.CodingRate = SX126x::LORA_CR_4_5;
	ModulationParams2.Params.LoRa.Bandwidth = SX126x::LORA_BW_500;
	ModulationParams2.Params.LoRa.SpreadingFactor = SX126x::LORA_SF7;

	SX126x::PacketParams_t PacketParams2;
	PacketParams2.PacketType = SX126x::PACKET_TYPE_LORA;
	auto &l = PacketParams2.Params.LoRa;
	l.PayloadLength = 253;
	l.HeaderType = SX126x::LORA_PACKET_FIXED_LENGTH;
	l.PreambleLength = 12;
	l.CrcMode = SX126x::LORA_CRC_ON;
	l.InvertIQ = SX126x::LORA_IQ_NORMAL;

	Radio.SetPacketType(SX126x::PACKET_TYPE_LORA);
	puts("SetPacketType done");
	Radio.SetModulationParams(&ModulationParams2);
	puts("SetModulationParams done");
	Radio.SetPacketParams(&PacketParams2);
	puts("SetPacketParams done");

	Radio.SetRfFrequency(433 * 1000000UL);
	puts("SetRfFrequency done");

	Radio.callbacks.txDone = []{
		puts("Wow TX done");
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

	while (1) {
		char buf[253] = "12345678\n";

		Radio.SendPayload((uint8_t *) buf, 64, 0);

		puts("SendPayload done");
		usleep(200 * 1000);
	}

}