/**
 * ESP32 zur Steuerung des Pools:
 * - 2 Temperaturfühler
 * - Interner Temperatur-Sensor
 * - 433MHz Sender für Pumpnsteuerung
 *
 * Wird über openHAB gesteuert.
 */

#include <Arduino.h>
#include <Homie.h>
//#include <ezTime.h>

#include "ConstantValues.hpp"

#include "DallasTemperatureNode.hpp"
#include "ESP32TemperatureNode.hpp"
#include "RelayModuleNode.hpp"
#include "RCSwitchNode.hpp"
#include "OperationModeNode.hpp"
#include "Rule.hpp"
#include "RuleManu.hpp"
#include "RuleAuto.hpp"
#include "RuleBoost.hpp"

#ifdef ESP32
const int PIN_DS_SOLAR = 15;  // Pin of Temp-Sensor Solar
const int PIN_DS_POOL  = 16;  // Pin of Temp-Sensor Pool
const int PIN_DHT11    = 17;

const int PIN_RSSWITCH = 18;  // Data-Pin of 433MHz Sender

const int PIN_RELAY_POOL  = 18;
const int PIN_RELAY_SOLAR = 19;
#else
const int PIN_DS_SOLAR = D5;  // Pin of Temp-Sensor Solar
const int PIN_DS_POOL  = D6;  // Pin of Temp-Sensor Pool
const int PIN_DHT11    = D7;

const int PIN_RELAY_POOL  = D1;
const int PIN_RELAY_SOLAR = D2;
#endif
const int TEMP_READ_INTERVALL = 60;  //Sekunden zwischen Updates der Temperaturen.

//#define LOCALTZ_POSIX "CET-1CEST,M3.4.0/2,M10.4.0/3"  // Time in Berlin
//Timezone tz;

HomieSetting<long> loopIntervalSetting("loop-interval", "The processing interval in seconds");

HomieSetting<long> temperatureMaxPoolSetting("temperature-max-pool", "Maximum temperature of solar");
HomieSetting<long> temperatureMinSolarSetting("temperature-min-solar", "Minimum temperature of solar");
HomieSetting<long> temperatureHysteresisSetting("temperature-hysteresis", "Temperature hysteresis");

HomieSetting<const char*> operationModeSetting("operation-mode", "Operational Mode");

DallasTemperatureNode solarTemperatureNode("solar-temp", "Solar Temperature", PIN_DS_SOLAR, TEMP_READ_INTERVALL);
DallasTemperatureNode poolTemperatureNode("pool-temp", "Pool Temperature", PIN_DS_POOL, TEMP_READ_INTERVALL);
#ifdef ESP32
ESP32TemperatureNode ctrlTemperatureNode("controller-temp", "Controller Temperature", TEMP_READ_INTERVALL);
#endif
RelayModuleNode poolPumpNode("pool-pump", "Pool Pump", PIN_RELAY_POOL);
RelayModuleNode solarPumpNode("solar-pump", "Solar Pump", PIN_RELAY_SOLAR);

OperationModeNode operationModeNode("operation-mode", "Operation Mode");

/**
 * Homie Setup handler.
 * Only called when wifi and mqtt are connected.
 */
void setupHandler() {

  // set mesurement intervals
  long _loopInterval = loopIntervalSetting.get();

  solarTemperatureNode.setMeasurementInterval(_loopInterval);
  poolTemperatureNode.setMeasurementInterval(_loopInterval);

  poolPumpNode.setMeasurementInterval(_loopInterval);
  solarPumpNode.setMeasurementInterval(_loopInterval);

#ifdef ESP32
  ctrlTemperatureNode.setMeasurementInterval(_loopInterval);
#endif

  String mode = String(operationModeSetting.get());
  operationModeNode.setMode(mode);
}

/**
 * Startup of controller.
 */
void setup() {
  Serial.begin(115200);

  while (!Serial) {
    ;  // wait for serial port to connect. Needed for native USB port only
  }

  Serial.println(F("-------------------------------------"));
  Serial.println(F(" Pool Controller                     "));
  Serial.println(F("-------------------------------------"));

  Homie_setFirmware("pool-controller", "1.0.0");  // The underscore is not a typo! See Magic bytes
  Homie_setBrand("smart-swimmingpool");

  //default intervall of sending Temperature values
  loopIntervalSetting.setDefaultValue(TEMP_READ_INTERVALL).setValidator([](long candidate) {
    return (candidate >= 0) && (candidate <= 300);
  });

  temperatureMaxPoolSetting.setDefaultValue(28.5).setValidator(
      [](long candidate) { return (candidate >= 0) && (candidate <= 30); });

  temperatureMinSolarSetting.setDefaultValue(50.0).setValidator(
      [](long candidate) { return (candidate >= 0) && (candidate <= 100); });

  temperatureHysteresisSetting.setDefaultValue(1.0).setValidator(
      [](long candidate) { return (candidate >= 0) && (candidate <= 10); });

  operationModeSetting.setDefaultValue("auto").setValidator([](const char* candidate) {
    return (strcmp(candidate, "auto")) || (strcmp(candidate, "manu")) || (strcmp(candidate, "boost"));
  });
  // set default configured OperationMode
  String mode = operationModeSetting.get();
  operationModeNode.setMode((char*)mode.c_str());

  // add the rules
  RuleAuto* autoRule = new RuleAuto(&solarPumpNode, &poolPumpNode);
  operationModeNode.addRule(autoRule);

  RuleManu* manuRule = new RuleManu();
  operationModeNode.addRule(manuRule);

  RuleBoost* boostRule = new RuleBoost(&solarPumpNode, &poolPumpNode);
  boostRule->setPoolMaxTemperatur(temperatureMaxPoolSetting.get());
  boostRule->setSolarMinTemperature(temperatureMinSolarSetting.get());  // TODO make changeable

  operationModeNode.addRule(boostRule);

  //Homie.disableLogging();
  Homie.setSetupFunction(setupHandler);
  Homie.setup();

  //etTime
  //setInterval(60);
  //setDebug(INFO);
  //tz.setPosix(LOCALTZ_POSIX);
  //tz.setLocation(F("de"));

  Homie.getLogger() << F("✔ main: Setup ready") << endl;
}

/**
 * Main loop of ESP.
 */
void loop() {

  Homie.loop();

  if (Homie.isConnected()) {
    //Homie.getLogger() << F("Germany: ") << tz.dateTime() << endl;
  }
  //ezTime
  //events();
}
