#include "ble/BLE.h"
#include "ble/Gap.h"
#include "ble/GattServer.h"
#include "mbed.h"
#include <array>
#include <cstdint>
#include <cstdio>


// Constants
static constexpr PinName SERVO_PIN = D5;
static constexpr PinName TRIG_PIN = D6;
static constexpr PinName ECHO_PIN = D3;
static constexpr int SERVO_PERIOD_MS = 20;
static constexpr int SERVO_PULSE_MIN_US = 500;
static constexpr int SERVO_PULSE_MAX_US = 2400;
static constexpr int DISTANCE_BUFFER_SIZE = 5;
static constexpr int LOOP_INTERVAL_MS = 100;

// UUIDs
static const UUID SERVICE_UUID("51311102-030e-485f-b122-f8f381aa84ed");
static const UUID ANGLE_CHAR_UUID("485f4145-52b9-4644-af1f-7a6b9322490f");
static const UUID DISTANCE_CHAR_UUID("0a924ca7-87cd-4699-a3bd-abdcd9cf126a");
static const UUID RUNNING_CHAR_UUID("8dd6a1b7-bc75-4741-8a26-264af75807de");

// Forward declarations
class HC_SR04;
class SG90Servo;

class HC_SR04 {
public:
  HC_SR04(PinName trig, PinName echo) : trigPin(trig), echoPin(echo) {
  }

  int measureDistance() {
    // Send trigger pulse
    trigPin = 0;
    wait_us(2);
    trigPin = 1;
    wait_us(10);
    trigPin = 0;
    
    // Wait for echo to start
    uint32_t startTime = 0;
    while (!echoPin) {
        startTime++;
        wait_us(1);
        if (startTime > 23200) return -1; // Timeout
    }
    
    // Wait for echo to end
    uint32_t pulseWidth = 0;
    while (echoPin) {
        pulseWidth++;
        wait_us(1);
        if (pulseWidth > 23200) return -1; // Timeout
    }
    
    // Calculate distance in cm
    return pulseWidth * 0.034 / 2;
  }

private:
  DigitalOut trigPin;
  DigitalIn echoPin;
};

class SG90Servo {
public:
  SG90Servo(PinName pin) : servoPin(pin) {
    servoPin.period_ms(SERVO_PERIOD_MS);
    setAngle(90);
  }

  void setAngle(int angle) {
    angle = std::max(0, std::min(angle, 180));
    int pulseWidth = SERVO_PULSE_MIN_US +
                     (angle * (SERVO_PULSE_MAX_US - SERVO_PULSE_MIN_US) / 180);
    servoPin.pulsewidth_us(pulseWidth);
  }

private:
  PwmOut servoPin;
};

class RadarService : ble::GattServer::EventHandler {
public:
  RadarService()
      : _angle_char(ANGLE_CHAR_UUID, 0), _distance_char(DISTANCE_CHAR_UUID, 0),
        _running_char(RUNNING_CHAR_UUID, 0),
        _radar_service(SERVICE_UUID, _radar_characteristics,
                       sizeof(_radar_characteristics) /
                           sizeof(_radar_characteristics[0])) {
    _radar_characteristics[0] = &_angle_char;
    _radar_characteristics[1] = &_distance_char;
    _radar_characteristics[2] = &_running_char;

    _angle_char.setReadAuthorizationCallback(
        this, &RadarService::authorize_client_read);
    _distance_char.setReadAuthorizationCallback(
        this, &RadarService::authorize_client_read);
    _running_char.setWriteAuthorizationCallback(
        this, &RadarService::authorize_client_write);
  }

  void start(BLE &ble, events::EventQueue &event_queue) {
    _ble = &ble;
    _event_queue = &event_queue;

    _ultrasonicSensor = std::make_unique<HC_SR04>(TRIG_PIN, ECHO_PIN);
    _servoControl = std::make_unique<SG90Servo>(SERVO_PIN);

    ble_error_t err = _ble->gattServer().addService(_radar_service);
    if (err) {
      printf("Error %u during radar service registration.\r\n", err);
      return;
    }

    _ble->gattServer().setEventHandler(this);

    _running_id =
        _event_queue->call_every(std::chrono::milliseconds(LOOP_INTERVAL_MS),
                                 this, &RadarService::update);
  }

private:
    void update() {
      if (!_running) {
        _servoControl->setAngle(_currentAngle);
        return;
      }

      int distance = _ultrasonicSensor->measureDistance();
      _servoControl->setAngle(_currentAngle);

      if (std::abs(distance - _lastDistance) > 2 ||
          std::abs(_currentAngle - _lastAngle) > 2) {
        _ble->gattServer().write(_distance_char.getValueHandle(),
                                 reinterpret_cast<uint8_t *>(&distance),
                                 sizeof(distance));
        _ble->gattServer().write(_angle_char.getValueHandle(),
                                 reinterpret_cast<uint8_t *>(&_currentAngle),
                                 sizeof(_currentAngle));
        _lastDistance = distance;
        _lastAngle = _currentAngle;
      }

      _currentAngle += _scanDirection;
      if (_currentAngle >= 180 || _currentAngle <= 0) {
        _scanDirection *= -1;
      }
    }
  

  void onDataWritten(const GattWriteCallbackParams &params) override {
    if (params.handle == _running_char.getValueHandle() && params.len == 1) {
      _running = params.data[0] != 0;
      printf("Radar running state changed to: %d\r\n", _running);
    }
  }

  void authorize_client_read(GattReadAuthCallbackParams *params) {
    params->authorizationReply = AUTH_CALLBACK_REPLY_SUCCESS;
  }

  void authorize_client_write(GattWriteAuthCallbackParams *params) {
    if (params->handle == _running_char.getValueHandle()) {
      params->authorizationReply = AUTH_CALLBACK_REPLY_SUCCESS;
    } else {
      params->authorizationReply =
          AUTH_CALLBACK_REPLY_ATTERR_WRITE_NOT_PERMITTED;
    }
  }

  BLE *_ble;
  events::EventQueue *_event_queue;
  GattCharacteristic _angle_char;
  GattCharacteristic _distance_char;
  GattCharacteristic _running_char;
  GattCharacteristic *_radar_characteristics[3];
  GattService _radar_service;
  std::unique_ptr<HC_SR04> _ultrasonicSensor;
  std::unique_ptr<SG90Servo> _servoControl;
  int _lastDistance = 0;
  int _lastAngle = 0;
  int _currentAngle = 0;
  int _scanDirection = 1;
  bool _running = true;
  int _running_id = 0;
};

static RadarService *g_radarService = nullptr;

void bleInitComplete(BLE::InitializationCompleteCallbackContext *context) {
  BLE &ble = context->ble;
  ble_error_t error = context->error;

  if (error != BLE_ERROR_NONE) {
    printf("BLE initialization failed.\r\n");
    return;
  }

  printf("BLE initialized\r\n");

  if (g_radarService == nullptr) {
    printf("RadarService not initialized.\r\n");
    return;
  }

  EventQueue *queue = mbed_event_queue();
  g_radarService->start(ble, *queue);

  constexpr size_t MAX_ADVERTISING_SIZE = 31;
  uint8_t adv_buffer[MAX_ADVERTISING_SIZE];
  ble::AdvertisingDataBuilder adv_data_builder(adv_buffer);

  adv_data_builder.setFlags();
  adv_data_builder.setName("NUCLEO-Radar");
  adv_data_builder.setAppearance(ble::adv_data_appearance_t::UNKNOWN);
  adv_data_builder.setLocalServiceList(mbed::make_Span(&SERVICE_UUID, 1));

  ble.gap().setAdvertisingPayload(ble::LEGACY_ADVERTISING_HANDLE,
                                  adv_data_builder.getAdvertisingData());

  ble.gap().setAdvertisingParameters(ble::LEGACY_ADVERTISING_HANDLE,
                                     ble::AdvertisingParameters());

  ble.gap().startAdvertising(ble::LEGACY_ADVERTISING_HANDLE);
}

void scheduleBleEventsProcessing(
    BLE::OnEventsToProcessCallbackContext *context) {
  mbed_event_queue()->call(
      Callback<void()>(&context->ble, &BLE::processEvents));
}

int main() {
  BLE &ble = BLE::Instance();
  RadarService radarService;
  g_radarService = &radarService;

  ble.onEventsToProcess(scheduleBleEventsProcessing);
  ble.init(bleInitComplete);

  EventQueue *queue = mbed_event_queue();
  queue->dispatch_forever();

  return 0;
}