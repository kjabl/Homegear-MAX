#include "BcTcCWm.h"
#include <homegear-base/BaseLib.h>
#include "../GD.h"
#include "../MAXDeviceTypes.h"

namespace MAX
{

BcTcCWm::BcTcCWm(uint32_t parentID, IPeerEventSink* EventHandler) : MAXPeer(parentID, EventHandler)
{
    init();
}

BcTcCWm::BcTcCWm(uint32_t id, int32_t address, std::string serialNumber, uint32_t parentID, IPeerEventSink* eventHandler) : MAXPeer(id, address, serialNumber, parentID, eventHandler)
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
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, e.what());
    }
    return false;
}

void BcTcCWm::loadVariables(BaseLib::Systems::ICentral* device, std::shared_ptr<BaseLib::Database::DataTable>& rows)
{
    try
    {
        MAXPeer::loadVariables(device, rows);
        for(BaseLib::Database::DataTable::iterator row = rows->begin(); row != rows->end(); ++row)
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
                _newMeasuredTemperature = row->second.at(3)->intValue;
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
        //startDutyCycle(calculateLastDutyCycleEvent());
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

/*void BcTcCWm::setMeasuredTemperature(float measuredTemperature, uint64_t &vdPeerID)
{
    try
    {
        //Valid range for measured temperature is 0 - 51.1 degree, so we have to check/correct this
        if (measuredTemperature < 0)
            measuredTemperature = 0;
        if (measuredTemperature > 51)
            measuredTemperature = 51;
        _newMeasuredTemperature = measuredTemperature;
        saveVariable(1005, _newMeasuredTemperature);

        // optional: integrate a check, if the temperature changed enough to send it again.

        // next step: encode it, build packet and send it
        // temperature is float but needs to be converted to int. So we need to multiply it by 10
        uint32_t temperatureAsInt = (int)(measuredTemperature * 10);
        // get paired valve drive to find out the actual desired temperature
        std::shared_ptr<MAXPeer> valveDrive = std::dynamic_pointer_cast<MAXPeer>(getPeer(1, vdPeerID, 3));
        // get desired temperature from paired valve drive, we need to convert it from float to int
        PParameter desiredParameter = valveDrive->valuesCentral[1]["SET_TEMPERATURE"].rpcParameter;
        BaseLib::PVariable desiredTemperatureVariable = valveDrive->getValueFromDevice(&desiredParameter, 1, false);

        // build payload
        std::vector<uint8_t> payload;
        payload.push_back(((temperatureAsInt & 0x100) >> 1) | (((uint32_t)(2 * desiredTemperatureVariable->floatValue)) & 0x7F));
        payload.push_back(temperatureAsInt & 0xFF);

        // now build and send the packet
        std::shared_ptr<MAXPacket> packet(new MAXPacket(valveDrive.getMessageCounter()[0], 0x42, 0, this->getAddress(), valveDrive->getAddress(), payload, valveDrive->getRXModes() & HomegearDevice::ReceiveModes::wakeOnRadio));
        std::shared_ptr<MAXCentral> central = std::dynamic_pointer_cast<MAXCentral>(valveDrive->getCentral());
        central->sendPacket(valveDrive->getPhysicalInterface(), packet);
    }
    catch (const std::exception &ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}*/

/*uint32_t BcTcCWm::encodeTemperature(float temperature){
    
    //logic to encode temperature logic

    // temperature is float but needs to be converted to int. So we need to multiply it by 10
    uint32_t temperatureAsInt = (int)(temperature*10);
    
    // get paired valve drive to find out the actual desired temperature
    MAXPeer valveDrive = this->getPeer(1);

    

    // get desired temperature from paired valve drive, we need to convert it from float to int
    PParameter desiredParameter = valveDrive.valuesCentral[1]["SET_TEMPERATURE"].rpcParameter;
    BaseLib::PVariable desiredTemperatureVariable = valveDrive.getValueFromDevice(&desiredParameter, 1, false);

    // First bit is 9th bit of temperature, rest is desired temperature
    uint32_t leftHalf = ((temperatureAsInt & 0x100)>>1) | (((uint32_t)(2*desiredTemperatureVariable->floatValue)) & 0x7F);
    // We need the last 8 bits from temperature
    uint32_t rightHalf = temperatureAsInt & 0xFF;
    // Now encoded temperature (to be used as payload) is built by appending the binary values of temp and temp2. We only need the last 8 bits from the right half
    uint32_t result = (leftHalf << 8) + (rightHalf & 0xFF);
    return result;
}*/

}