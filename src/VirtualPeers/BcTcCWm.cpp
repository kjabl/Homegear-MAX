#include "BcTcCWM.h"
#include <homegear-base/BaseLib.h>
#include "../GD.h"

namespace MAX
{

BcTcCWm::BcTcCWm(uint32_t parentID, IPeerEventSink* EventHandler) : MAXPeer(parentID, EventHandler)
{
    init();
    startDutyCycle(-1); //Peer is newly created
}

BcTcCWm::BcTcCWm(int32_t id, int32_t address, std::string serialNumber, uint32_t parentID, IPeerEventSink* eventHandler) : MAXPeer(id, address, serialNumber, parentID, eventHandler)
{
    init();
}

BcTcCWm::~BcTcCWm()
{
    dispose();
}

void BcTcCWm::init()
{
    try
    {
        _stopDutyCycleThread = false;
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void BcTcCWm::dispose()
{
    try
    {
        _stopDutyCycleThread = true;
        _bl->threadManager.join(_dutyCycleThread);
        _bl->threadManager.join(_sendDutyCyclePacketThread);
        MAXPeer::dispose();
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void BcTcCWm::worker()
{
    //Is also empty in my "template" HmCcTc.cpp in Homegear/HomeMaticBidCos/src/VirtualPeers
}

bool BcTcCWm::load(BaseLib::Systems::ICentral* device)
{
    try
    {
        MAXPeer::load(device);
        serviceMessages->endUnreach();
        if(!_rpcDevice)
        {
            GD::out.printError("Error: Could not find RPC device for peer with ID " + std::to_string(_peerID));
            return true;
        }
        _rpcDevice->receiveModes = BaseLib::DeviceDescription::HomegearDevice::ReceiveModes::Enum::always;
        _rpcDevice->timeout = 0;
        return true;
    }
    catch(const std::exception& e)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    return false;
}

void BcTcCWm::loadVariables(BaseLib::Systems::ICentral* device, std::shared_ptr<BaseLib::Database::DataTable>& rows)
{
    try
    {
        MAXPeer::loadVariables(device, rows);
        for(BaseLib::Database::DataTable::iterator row = row->begin(); row != rows->end(); ++row)
        {
            _variableDatabaseIDs[row->second.at(2)->intValue] = row->second.at(0)->intValue;
            switch(row->second.at(2)->intValue)
            {
            case 1000:
                _currentDutyCycleDeviceAddress = row->second.at(3)->intValue;
                break;
                //TODO: ID der Werte klären und entsprechend anpassen
            case 1004:
                _measuredTemperature = row->second.at(3)->intValue;
                break;
            case 1005:
                _newMeasuredTemperatur = row->second.at(3)->intValue;
                break;
            case 1006:
                _lastDutyCycleEvent = row->second.at(3)->intValue;
                break;
            case 1007:
                _dutyCycleMessageCounter = (uint8_t)row->second.at(3)->intValue;
                break;
            }
        }
        setDeviceType((uint32_t)DeviceType::BCTCCWM4);
        startDutyCycle(calculateLastDutyCycleEvent());
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }   
}

void BcTcCWm::saveVariables()
{
    try
    {
        MAXPeer::saveVariables();
        saveVariable(1000, _currentDutyCycleDeviceAddress);
        saveVariable(1004, _measuredTemperature);
        saveVariable(1005, _newMeasuredTemperature);
        saveVariable(1006, _lastDutyCycleEvent);
        saveVariable(1007, (int64_t)_dutyCycleMessageCounter);
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    
}


void BcTcCWm::setMeasuredTemperature(int32_t measuredTemperature)
{
    try
    {
    //Valid range for measured temperature is 0 - 51.1 degree, so we have to check/correct this
    if(measuredTemperature < 0) measuredTemperature = 0;
    if (measuredTemperature > 51) measuredTemperature = 51;
    _newMeasuredTemperature = measuredTemperature;
    saveVariable(1005, _newMeasuredTemperature);   
    }
    catch (const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

uint8_t BcTCCWm::encodeTemperature(int32_t temperature){
    //Fill with logic to encode temperature
}

}