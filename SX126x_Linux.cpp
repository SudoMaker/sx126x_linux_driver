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

SX126x_Linux::SX126x_Linux(const std::string &spi_dev_path, uint16_t gpio_dev_num, SX126x_Linux::PinConfig pin_config) :
	pin_cfg(pin_config),
	RadioSpi(spi_dev_path, pin_config.nss >= 0 ? [this](bool cs){if (cs) RadioNss->write(0); else RadioNss->write(1);} : std::function<void(bool)>(), SPI_MODE_0, 8, 500000),
	RadioGpio(gpio_dev_num),
	Busy(RadioGpio.line(pin_cfg.busy, GPIO::LineMode::Input, 0, "SX126x BUSY")),
	RadioReset(RadioGpio.line(pin_cfg.nrst, GPIO::LineMode::Output, 1, "SX126x NRESET"))
{
	if (pin_config.nss >= 0) {
		RadioNss = RadioGpio.line(pin_cfg.nss, GPIO::LineMode::Output, 1, "SX126x NSS");
	}

	if (pin_config.tx_en >= 0) {
		TxEn = RadioGpio.line(pin_cfg.tx_en, GPIO::LineMode::Output, 0, "SX126x TXEN");
	}

	if (pin_config.rx_en >= 0) {
		RxEn = RadioGpio.line(pin_cfg.rx_en, GPIO::LineMode::Output, 0, "SX126x RXEN");
	}

	int i = 1;
	for (auto it : {pin_config.dio1, pin_config.dio2, pin_config.dio3}) {
		std::string label = "SX126x DIO";
		label += std::to_string(i);
		i++;

		if (it != -1) {
			RadioGpio.add_event(it, GPIO::LineMode::Input, GPIO::EventMode::RisingEdge,
					   [this](GPIO::EventType t, uint64_t) {
						   if (t == GPIO::EventType::RisingEdge)
							   ProcessIrqs();
					   }, label);
		}
	}
}

void SX126x_Linux::SetSpiSpeed(uint32_t hz) {
	RadioSpi.set_max_speed_hz(hz);
}

void SX126x_Linux::SetExternalLock(std::mutex &m) {
	ExtLock = &m;
}

void SX126x_Linux::StartIrqHandler(int __prio) {
	IrqThread = std::thread([this, __prio](){
		sched_param param;
		param.sched_priority = __prio;
		pthread_setschedparam(pthread_self(), SCHED_RR, &param);
		RadioGpio.run_eventlistener();
	});
}

void SX126x_Linux::StopIrqHandler() {
	RadioGpio.stop_eventlistener();
	IrqThread.join();
}

uint8_t SX126x_Linux::HalGpioRead(SX126x::GpioPinFunction_t func) {
	switch (func) {
		case SX126x::GPIO_PIN_BUSY:
			return Busy.read();
		default:
			return 0;
	}
}

void SX126x_Linux::HalGpioWrite(SX126x::GpioPinFunction_t func, uint8_t value) {
	switch (func) {
		case SX126x::GPIO_PIN_RESET:
			RadioReset.write(value);
		default:
			return;
	}
}

void SX126x_Linux::HalSpiTransfer(uint8_t *buffer_in, const uint8_t *buffer_out, uint16_t size) {
	if (ExtLock) {
		std::lock_guard<std::mutex> lg(*ExtLock);
		RadioSpi.transfer(buffer_out, buffer_in, size);
	} else {
		RadioSpi.transfer(buffer_out, buffer_in, size);
	}
}

void SX126x_Linux::HalPreTx() {
	if (TxEn) {
		TxEn->write(1);
	}
}

void SX126x_Linux::HalPreRx() {
	if (RxEn) {
		RxEn->write(1);
	}
}

void SX126x_Linux::HalPostTx() {
	if (TxEn) {
		TxEn->write(0);
	}
}

void SX126x_Linux::HalPostRx() {
	if (RxEn) {
		RxEn->write(0);
	}
}

