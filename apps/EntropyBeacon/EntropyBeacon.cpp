#include "EntropyBeacon.h"

EntropyBeaconApp::EntropyBeaconApp() {}
EntropyBeaconApp::~EntropyBeaconApp() {}

bool EntropyBeaconApp::detectAnomaly(float value) {
    return false;
}

bool EntropyBeaconApp::handleTouch(TouchPoint touch) {
    return false;
}

bool EntropyBeaconApp::initialize() {
    return false;
}

bool EntropyBeaconApp::startRecording() {
    return false;
}

const uint8_t* EntropyBeaconApp::getIcon() const {
    return {};
}

float EntropyBeaconApp::getCurrentEntropy() const {
    return 0.0f;
}

uint16_t EntropyBeaconApp::getBufferSize() const {
    return 0;
}

void EntropyBeaconApp::calculateSampleInterval() {
    
}

void EntropyBeaconApp::cleanup() {
    
}

void EntropyBeaconApp::drawControls() {
    
}

void EntropyBeaconApp::drawOscilloscope() {
    
}

void EntropyBeaconApp::drawSpectrum() {
    
}

void EntropyBeaconApp::loadConfiguration() {
    
}

void EntropyBeaconApp::logEventToSD(String eventType, float value) {
    
}

void EntropyBeaconApp::performSimpleFFT() {
    
}

void EntropyBeaconApp::render() {
    
}

void EntropyBeaconApp::sampleEntropy() {
    
}

void EntropyBeaconApp::saveConfiguration() {
    
}

void EntropyBeaconApp::stopRecording() {
    
}

void EntropyBeaconApp::update() {
    
}

void EntropyBeaconApp::updateDACOutput() {
    
}

void EntropyBeaconApp::updateStatistics(float value) {
    
}

void EntropyBeaconApp::writeDataToSD(uint16_t value, float normalized, bool isAnomaly) {
    
}

