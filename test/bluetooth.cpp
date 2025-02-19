/* mbed Microcontroller Library
 * Copyright (c) 2017-2019 ARM Limited
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

#include "platform/Callback.h"
#include "events/EventQueue.h"
#include "ble/BLE.h"
#include "gatt_server_process.h"
#include "mbed-trace/mbed_trace.h"
#include <cstdio>

PwmOut servoPin(D5);
DigitalOut trigPin(D6); 
InterruptIn echoPin(D3); 
EventQueue queue;
Thread event_thread;

using mbed::callback;
using namespace std::literals::chrono_literals;



/**
 * A Clock service that demonstrate the GattServer features.
 *
 * The clock service host three characteristics that model the current hour,
 * minute and second of the clock. The value of the second characteristic is
 * incremented automatically by the system.
 *
 * A client can subscribe to updates of the clock characteristics and get
 * notified when one of the value is changed. Clients can also change value of
 * the second, minute and hour characteristric.
 */
class ClockService : public ble::GattServer::EventHandler {
public:
    ClockService() :
        _angle_char("485f4145-52b9-4644-af1f-7a6b9322490f", 0),
        _distance_char("0a924ca7-87cd-4699-a3bd-abdcd9cf126a", 0),
        _running_char("8dd6a1b7-bc75-4741-8a26-264af75807de", 0),
        _clock_service(
            /* uuid */ "51311102-030e-485f-b122-f8f381aa84ed",
            /* characteristics */ _clock_characteristics,
            /* numCharacteristics */ sizeof(_clock_characteristics) /
                                     sizeof(_clock_characteristics[0])
        )
    {
        /* update internal pointers (value, descriptors and characteristics array) */
        _clock_characteristics[0] = &_angle_char;
        _clock_characteristics[1] = &_distance_char;
        _clock_characteristics[2] = &_running_char;

        /* setup authorization handlers */
        _angle_char.setWriteAuthorizationCallback(this, &ClockService::authorize_client_write);
        _distance_char.setWriteAuthorizationCallback(this, &ClockService::authorize_client_write);
        _running_char.setWriteAuthorizationCallback(this, &ClockService::authorize_client_write);
    }

    void start(BLE &ble, events::EventQueue &event_queue)
    {
        _server = &ble.gattServer();
        _event_queue = &event_queue;

        // Configure servo PWM: 20ms period and initial position
        servoPin.period_ms(20); // 20ms period for standard servos
        servoPin.pulsewidth_us(1500); // Neutral position (1.5ms pulse)

        //event_thread.start(callback(_event_queue, &EventQueue::dispatch_forever));

        //echoPin.rise(callback(this, &ClockService::on_echo_rise));
        //echoPin.fall(callback(this, &ClockService::on_echo_fall));

        printf("Registering demo service\r\n");
        ble_error_t err = _server->addService(_clock_service);

        if (err) {
            printf("Error %u during demo service registration.\r\n", err);
            return;
        }

        /* register handlers */
        _server->setEventHandler(this);

        running_id = _event_queue->call_every(30ms, callback(this, &ClockService::loop));
    }


    /* GattServer::EventHandler */
private:

    void on_echo_rise() {
        _event_queue->call(callback(this, &ClockService::echo_rise));
    }

    void echo_rise() {
        if (timer_started) return;
        timer_started = true;
        timer.reset();
        timer.start();
        printf("rise up\n\n");
    }

    void on_echo_fall() {
        _event_queue->call(callback(this, &ClockService::echo_fall));
    }

    void echo_fall() {
        if (!timer_started) return;
        timer_started = false;
        timer.stop();
        distance = calculateDistance();
        
    }

    int calculateDistance() {
  
    // Calculate duration in microseconds
    long duration = timer.elapsed_time().count(); 

    printf("time: %ld\n\n", duration);

    // Calculate distance: speed of sound is ~343 m/s or 0.0343 cm/µs
    int distance = duration * 0.0343 / 2;

    return distance; // Distance in cm
    }

    /**
     * Handler called when a notification or an indication has been sent.
     */
    void onDataSent(const GattDataSentCallbackParams &params) override
    {
        printf("sent updates\r\n");
    }

    /**
     * Handler called after an attribute has been written.
     */
    void onDataWritten(const GattWriteCallbackParams &params) override
    {
        printf("data written:\r\n");
        printf("connection handle: %u\r\n", params.connHandle);
        printf("attribute handle: %u", params.handle);

        if (params.handle == _running_char.getValueHandle()) {
            printf("Value received: \r\n");
            if (params.data[0] == 0) {
                _event_queue->cancel(running_id);
                running_id = 0;
            }
            else if (params.data[0] != 0 && running_id == 0) {
                running_id = _event_queue->call_every(30ms, callback(this, &ClockService::loop));
            }
        }

        printf("write operation: %u\r\n", params.writeOp);
        printf("offset: %u\r\n", params.offset);
        printf("length: %u\r\n", params.len);
        printf("data: ");

        for (size_t i = 0; i < params.len; ++i) {
            printf("%02X", params.data[i]);
        }

        printf("\r\n");
    }

    /**
     * Handler called after an attribute has been read.
     */
    void onDataRead(const GattReadCallbackParams &params) override
    {
        printf("data read:\r\n");
        printf("connection handle: %u\r\n", params.connHandle);
        printf("attribute handle: %u", params.handle);
        if (params.handle == _angle_char.getValueHandle()) {
            printf(" (hour characteristic)\r\n");
        } else if (params.handle == _distance_char.getValueHandle()) {
            printf(" (minute characteristic)\r\n");
        } else if (params.handle == _running_char.getValueHandle()) {
            printf(" (second characteristic)\r\n");
        } else {
            printf("\r\n");
        }
    }

    /**
     * Handler called after a client has subscribed to notification or indication.
     *
     * @param handle Handle of the characteristic value affected by the change.
     */
    void onUpdatesEnabled(const GattUpdatesEnabledCallbackParams &params) override
    {
        printf("update enabled on handle %d\r\n", params.attHandle);
    }

    /**
     * Handler called after a client has cancelled his subscription from
     * notification or indication.
     *
     * @param handle Handle of the characteristic value affected by the change.
     */
    void onUpdatesDisabled(const GattUpdatesDisabledCallbackParams &params) override
    {
        printf("update disabled on handle %d\r\n", params.attHandle);
    }

    /**
     * Handler called when an indication confirmation has been received.
     *
     * @param handle Handle of the characteristic value that has emitted the
     * indication.
     */
    void onConfirmationReceived(const GattConfirmationReceivedCallbackParams &params) override
    {
        printf("confirmation received on handle %d\r\n", params.attHandle);
    }

private:
    /**
     * Handler called when a write request is received.
     *
     * This handler verify that the value submitted by the client is valid before
     * authorizing the operation.
     */
    void authorize_client_write(GattWriteAuthCallbackParams *e)
    {
        printf("characteristic %u write authorization\r\n", e->handle);

        if (e->offset != 0) {
            printf("Error invalid offset\r\n");
            e->authorizationReply = AUTH_CALLBACK_REPLY_ATTERR_INVALID_OFFSET;
            return;
        }

        if (e->len != 1) {
            printf("Error invalid len\r\n");
            e->authorizationReply = AUTH_CALLBACK_REPLY_ATTERR_INVALID_ATT_VAL_LENGTH;
            return;
        }

        if ((e->data[0] >= 60) ||
            ((e->data[0] >= 24) && (e->handle == _angle_char.getValueHandle()))) {
            printf("Error invalid data\r\n");
            e->authorizationReply = AUTH_CALLBACK_REPLY_ATTERR_WRITE_NOT_PERMITTED;
            return;
        }

        e->authorizationReply = AUTH_CALLBACK_REPLY_SUCCESS;
    }

        void loop(void)
    {
        printf("loop");
        trigPin.write(0);          // Ensure trigger is low
        wait_us(2);           // Wait for a short period
        trigPin.write(1);          // Trigger high for 10µs
        wait_us(10);          
        trigPin.write(0);          // Trigger low
        //printf("triggered");

        // Wait for the echo to start
        while (!echoPin.read()) {};

        // Start timing when echo starts
        timer.reset();
        timer.start();

        // Wait for the echo to end
        while (echoPin.read()) {};

        timer.stop();

        // Calculate duration in microseconds
        long duration = timer.elapsed_time().count(); 

        printf("time: %ld\n\n", duration);
        
        //if (direction) {
        //    angle += 1;
        //} else {
        //    angle -= 1;
        //}
//
        //float pulseWidth = 1000 + (angle * 1000.0 / 180.0); // Map degrees to pulse width (1ms to 2ms)
        //servoPin.pulsewidth_us(pulseWidth);
        //if (angle == 180 || angle == 0) {
        //    direction = !direction;
        //}
//
        //_distance_char.set(* _server, distance);
        //_angle_char.set(* _server, angle);

    }


private:
    /**
     * Read, Write, Notify, Indicate  Characteristic declaration helper.
     *
     * @tparam T type of data held by the characteristic.
     */
    template<typename T>
    class ReadWriteNotifyIndicateCharacteristic : public GattCharacteristic {
    public:
        /**
         * Construct a characteristic that can be read or written and emit
         * notification or indication.
         *
         * @param[in] uuid The UUID of the characteristic.
         * @param[in] initial_value Initial value contained by the characteristic.
         */
        ReadWriteNotifyIndicateCharacteristic(const UUID & uuid, const T& initial_value) :
            GattCharacteristic(
                /* UUID */ uuid,
                /* Initial value */ &_value,
                /* Value size */ sizeof(_value),
                /* Value capacity */ sizeof(_value),
                /* Properties */ GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_READ |
                                 GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_WRITE |
                                 GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_NOTIFY |
                                 GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_INDICATE,
                /* Descriptors */ nullptr,
                /* Num descriptors */ 0,
                /* variable len */ false
            ),
            _value(initial_value) {
        }

        /**
         * Get the value of this characteristic.
         *
         * @param[in] server GattServer instance that contain the characteristic
         * value.
         * @param[in] dst Variable that will receive the characteristic value.
         *
         * @return BLE_ERROR_NONE in case of success or an appropriate error code.
         */
        ble_error_t get(GattServer &server, T& dst) const
        {
            uint16_t value_length = sizeof(dst);
            return server.read(getValueHandle(), &dst, &value_length);
        }

        /**
         * Assign a new value to this characteristic.
         *
         * @param[in] server GattServer instance that will receive the new value.
         * @param[in] value The new value to set.
         * @param[in] local_only Flag that determine if the change should be kept
         * locally or forwarded to subscribed clients.
         */
        ble_error_t set(GattServer &server, const uint8_t &value, bool local_only = false) const
        {
            return server.write(getValueHandle(), &value, sizeof(value), local_only);
        }

    private:
        uint8_t _value;
    };

private:
    GattServer *_server = nullptr;
    events::EventQueue *_event_queue = nullptr;

    int angle = 0;
    bool direction = true;
    bool running = true;
    int running_id = 0;
    int distance = 0;
    bool timer_started = false;
    Timer timer;

    GattService _clock_service;
    GattCharacteristic* _clock_characteristics[3];

    ReadWriteNotifyIndicateCharacteristic<uint8_t> _angle_char;
    ReadWriteNotifyIndicateCharacteristic<uint8_t> _distance_char;
    ReadWriteNotifyIndicateCharacteristic<uint8_t> _running_char;
};

int main()
{
    mbed_trace_init();

    BLE &ble = BLE::Instance();
    events::EventQueue event_queue;
    ClockService demo_service;

    /* this process will handle basic ble setup and advertising for us */
    GattServerProcess ble_process(event_queue, ble);

    /* once it's done it will let us continue with our demo */
    ble_process.on_init(callback(&demo_service, &ClockService::start));

    ble_process.start();

    return 0;
}
