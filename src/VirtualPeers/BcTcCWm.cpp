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
            case 1010:
                _measuredTemperature = row->second.at(3)->intValue;
                break;
            case 1011:
                _newMeasuredTemperature = row->second.at(3)->intValue;
                break;
            }
        }
        setDeviceType((uint32_t)DeviceType::BCTCCWM4);
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
        saveVariable(1010, _measuredTemperature);
        saveVariable(1011, _newMeasuredTemperature);
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    
}

void BcTcCWm::handleWakeUp(int32_t valveDriveAddress, uint8_t messageCounter) {

    try
    {
        // got WakeUp Packet from Valve Drive, need to answer with a normal package so it knows the wallthermostats still exists
        std::shared_ptr<MAXPeer> valveDrive = dynamic_pointer_cast<MAXPeer>(getPeer(1, valveDriveAddress, 3));
        
        // aktuelle desiredTemperature vom Gerät besorgen -> noch erfragen wie das funktioniert
        int32_t desiredTemperature = (int) (getDesiredTemperature(valveDriveAddress)*2);

        // If we are quick enough we don't need WoR so we can save credits
        // lets do it like fhem does:
        std::vector<uint8_t> payload;
        payload.push_back(71937); // FHEM uses "011901" as a hex string. This is the corresponding value
        std::shared_ptr<MAXPacket> packet = new MAXPacket(messageCounter, 0x02, 0x02, _address, valveDriveAddress, payload, false);
        std::shared_ptr<MAXCentral> central = std::dynamic_pointer_cast<MAXCentral>(getCentral());
        central->sendPacket(getPhysicalInterface(), packet);
    }
    catch (const std::exception &ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

float BcTcCWm::getDesiredTemperature(int32_t valveDriveAddress) {
    // Method is used to get the desiredTemperature from schedule in device

    // Perhaps this could be done easier
    time_t timer;
    struct tm * timeinfo;
    time (&timer);
    timeinfo = localtime (&timer);
    int16_t wday = timeinfo->tm_wday;   
    std::string daystr;

    // get ValveDrivePeer
    std::shared_ptr<MAXPeer> valveDrive = dynamic_pointer_cast<MAXPeer>(getPeer(1, valveDriveAddress, 3));

    BaseLib::DeviceDescription::PParameterGroup paramset = valveDrive->getParameterSet(0, BaseLib::DeviceDescription::ParameterGroup::Type::Enum::config);
    // get the actual day
    switch (wday)
    {
    case 0:
        daystr = "MONDAY"
        break;
    case 1:
        daystr = "TUESDAY"
        break;
    case 2:
        daystr = "WEDNESDAY"
        break;
    case 3:
        daystr = "THURSDAY"
        break;
    case 4:
        daystr = "FRIDAY"
        break;
    case 5:
        daystr = "SATURDAY"
        break;
    case 6:
        daystr = "SUNDAY"
        break;
    }

    
    // find the right timespot
    BaseLib::DeviceDescription::Parameter param;
    int32_t minutes = timeinfo->tm_min;
    float desiredTemp;
    for (size_t i = 1; i < 14; i++)
    {
        param = paramset->getParameter("ENDTIME_"+daystr+"_"+std::to_string(i));
        if(valveDrive->getValueFromDevice(&param, 0, true)->integerValue > minutes) {
            // found the timespot, getting temperature now
            param = paramset->getParameter("TEMPERATURE_"+daystr+"_"+std::to_string(i));
            desiredTemp = valveDrive->getValueFromDevice(&param, 0, true);
            break;
        }
    }
    return desiredTemp; 
}

std::shared_ptr<MAXPacket> BcTcCWm::buildFakePacket(int32_t measuredTemperature, int32_t desiredTemperature,
                                                int32_t valveDriveAddress, bool burst) {
    try
    {
        // Build payload
        std::vector<uint8_t> payload;
        payload.push_back(0x00); // adding two zeros for what FHEM calls groupID
        payload.push_back(((measuredTemperature & 0x100) >> 1) | (((uint32_t)(2 * desiredTemperature)) & 0x7F));
        payload.push_back(measuredTemperature & 0xFF);

        // build packet
        std::shared_ptr<MAXPacket> packet(new MAXPacket(_messageCounter, 0x42, 0, _address, valveDriveAddress, payload, burst));
        return packet;
    }
    catch (const std::exception &ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

std::shared_ptr<MAXPacket> BcTcCWm::buildFakePacket(float measuredTemperature, float desiredTemperature, int32_t valveDriveAddress, bool burst) {
    int32_t measuredTemperatureAsInt = (int)(measuredTemperature*10);
    int32_t desiredTemperatureAsInt = (int)(desiredTemperature*2);
    buildFakePacket(measuredTemperatureAsInt, desiredTemperatureAsInt, valveDriveAddress, burst);
}

void BcTcCWm::setMeasuredTemperature(float measuredTemperature, uint64_t &vdPeerID, PRpcClientInfo clientInfo)
{
    try
    {
        //Valid range for measured temperature is 0 - 51.1 degree, so we have to check/correct this
        if (measuredTemperature < 0)
            measuredTemperature = 0;
        if (measuredTemperature > 51)
            measuredTemperature = 51;
        
        _newMeasuredTemperature = (int)(10*measuredTemperature);

        // checking if temperature changed enough. I should integrate a config parameter for this. But I don't know how at the moment.
        if(abs(measuredTemperature-_measuredTemperature) < 0.2) {
            GD::out.printInfo("New measured temperature for Valve drive with Peer ID" + std::to_string(vdPeerID) + "is too small. Won't send it to save credits. Old: " + std::to_string(_measuredTemperature) + " New: " + std::to_string(_newMeasuredTemperature) + ".");
            return;
        }
        // temperature is float but needs to be converted to int. So we need to multiply it by 10
        saveVariable(1011, _newMeasuredTemperature);

        // next step: encode it, build packet and send it
        // get paired valve drive to find out the actual desired temperature
        std::shared_ptr<MAXPeer> valveDrive = std::dynamic_pointer_cast<MAXPeer>(getPeer(1, vdPeerID, 3));
        // get desired temperature from paired valve drive, we need to convert it from float to int
        BaseLib::PVariable desiredTemperatureVariable = getValue(clientInfo, 1, "SET_TEMPERATURE", false, true); // how can I do this in a different way???
        
        // now build and send the packet
        std::shared_ptr<MAXPacket> packet = buildFakePacket(measuredTemperature, desiredTemperatureVariable->floatValue, valveDrive->getAddress(), valveDrive->getRXModes() & HomegearDevice::ReceiveModes::wakeOnRadio));
        GD::out.printDebug("Sending fake Wallthermostat packet from Peer: " + std::to_string(_peerID) + " to valve drive with ID: " + valveDrive->getAddress() + "with desired Temperature: " + std::to_string(desiredTemperatureVariable->floatValue) + " and measured temperature: " + std::to_string(measuredTemperature));
        std::shared_ptr<MAXCentral> central = std::dynamic_pointer_cast<MAXCentral>(valveDrive->getCentral());
        central->sendPacket(valveDrive->getPhysicalInterface(), packet);
        _messageCounter++;
    }
    catch (const std::exception &ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

}