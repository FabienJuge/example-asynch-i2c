/* mbed Microcontroller Library
 * Copyright (c) 2015 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdio.h>
#include <stdint.h>

#include "mbed-drivers/mbed.h"
#include "minar/minar.h"
#include "core-util/Event.h"
#include "mbed-drivers/v2/I2C.hpp"
#if DEVICE_I2C && DEVICE_I2C_ASYNCH

#if !defined(YOTTA_CFG_HARDWARE_TEST_PINS_I2C_SDA) || !defined(YOTTA_CFG_HARDWARE_TEST_PINS_I2C_SCL)
#error This example requires I2C test pins to be defined. Please define the hardware.test-pins.i2c.sda/scl \
    yotta confing values
#endif

using namespace minar;
using namespace mbed::drivers;

const char pattern[] = {0x66, (char)~0x66, 0x00, 0xFF, 0xA5, 0x5A, 0xF0, 0x0F, 0x33, 0xAA, 0xF0, 0x0F, 0xFF};
const int eeprom_address = 0xA0;
const size_t buffer_size = sizeof(pattern);

class I2CTest {

public:
    I2CTest() : i2c(YOTTA_CFG_HARDWARE_TEST_PINS_I2C_SDA, YOTTA_CFG_HARDWARE_TEST_PINS_I2C_SCL) {
        i2c.frequency(400000);
    }

    void start() {
        printf("Starting transfer test \r\n");
        init_tx_buffer();
        init_rx_buffer();
        set_eeprom_access_address(0x100);
        set_tx_pattern();
        i2c.transfer_to(eeprom_address)
            .tx(tx_data, sizeof(tx_data))
            .on(I2C_EVENT_ALL, v2::I2C::event_callback_t(this, &I2CTest::write_data_complete))
            .apply();
    }

private:
    void set_eeprom_access_address(unsigned int address) {
        tx_data[0] = (address >> 8) & 0xFF; // high byte address
        tx_data[1] = address & 0xFF;        // low byte address
    }

    void write_data_complete(v2::I2CTransaction *t, uint32_t narg) {
        (void)t;
        printf("Writing DONE, event is %lu\r\n", narg);
        i2c.transfer_to(eeprom_address)
            .tx(nullptr,0)
            .on(I2C_EVENT_ALL, v2::I2C::event_callback_t(this, &I2CTest::slave_ready))
            .apply();
    }

    void slave_ready(v2::I2CTransaction *t, uint32_t narg) {
        (void)t;
        if (narg == I2C_EVENT_TRANSFER_COMPLETE) {
            // slave ack, send more data
            i2c.transfer_to(eeprom_address)
                .tx(nullptr,0)
                .on(I2C_EVENT_ALL, v2::I2C::event_callback_t(this, &I2CTest::read_data_cb))
                .apply();
        } else {
            // slave not ready, retry
            i2c.transfer_to(eeprom_address)
                .tx(nullptr,0)
                .on(I2C_EVENT_ALL, v2::I2C::event_callback_t(this, &I2CTest::slave_ready))
                .apply();
        }
    }

    void read_data_cb(v2::I2CTransaction *t, uint32_t narg) {
        (void)t;
        printf("Slave is ready for reading, event is %lu\r\n", narg);
        set_eeprom_access_address(0x100);
        i2c.transfer_to(eeprom_address)
            .tx(tx_data,2)
            .rx(rx_data, sizeof(rx_data))
            .on(I2C_EVENT_ALL, v2::I2C::event_callback_t(this, &I2CTest::compare_data_cb))
            .apply();
    }

    void compare_data_cb(v2::I2CTransaction *t, uint32_t narg) {
        (void)t;
        printf("Reading DONE, event is %lu\r\n", narg);
        // received buffer match with pattern
        int rc = memcmp(pattern, rx_data, sizeof(rx_data));
        if (rc == 0) {
            printf("Read data match with written data, event is %lu\r\n", narg);
        } else {
            printf("Read data doesn't match with written data, event is %lu\r\n", narg);
            for (uint32_t i = 0; i < sizeof(rx_data); i++) {
                printf("Written:0x%x Read:0x%x \n\r", pattern[i], rx_data[i]);
            }
        }
        printf("**** Test done ****\r\n");
    }

    void init_tx_buffer() {
        for (uint32_t i = 0; i < sizeof(tx_data); i++) {
            tx_data[i] = 0;
        }
    }

    void init_rx_buffer() {
        for (uint32_t i = 0; i < sizeof(rx_data); i++) {
            rx_data[i] = 0;
        }
    }

    void set_tx_pattern() {
        for (uint32_t i = 0; i < buffer_size; i++) {
            tx_data[i + 2] = pattern[i];
        }
    }

private:
    v2::I2C i2c;
    char tx_data[buffer_size + 2];
    char rx_data[buffer_size];
};


void app_start(int, char*[]) {
    static I2CTest test;
    // set 115200 baud rate for stdout
    get_stdio_serial().baud(115200);
    Scheduler::postCallback(mbed::util::FunctionPointer0<void>(&test, &I2CTest::start).bind());
}

#else

void app_start(int, char*[]) {
    // set 115200 baud rate for stdout
    get_stdio_serial().baud(115200);
    printf("The target does not support I2C asynch API.\r\n");
}
#endif
