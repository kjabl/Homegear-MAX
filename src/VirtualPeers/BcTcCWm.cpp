#include "BcTcCWm.h"
#include <homegear-base/BaseLib.h>
#include "MAXCentral.h"
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

    GD::out.printDebug("Starting to handle debug packet.");
    try
    {
        // got WakeUp Packet from Valve Drive, need to answer with a normal package so it knows the wallthermostats still exists
        std::shared_ptr<MAXPeer> valveDrive = std::dynamic_pointer_cast<MAXPeer>(getPeer(1, valveDriveAddress, 3));
        if(!valveDrive) {
            GD::out.printDebug("Didn't find real valve drive to handle wakeup packet for.");
            return;
        }
        
        // aktuelle desiredTemperature vom Gerät besorgen -> noch erfragen wie das funktioniert
        int32_t desiredTemperature = (int) (valveDrive->getTemperatureFromWeekplan()*2);
        GD::out.printDebug("Desired temperature from weekplan is: "+std::to_string(desiredTemperature));

        // If we are quick enough we don't need WoR so we can save credits
        // lets do it like fhem does:
        std::vector<uint8_t> payload;
        payload.push_back(71937); // FHEM uses "011901" as a hex string. This is the corresponding value. mode=auto
        payload.push_back(desiredTemperature & 0x7F);
        std::shared_ptr<MAXPacket> packet (new MAXPacket(messageCounter, 0x02, 0x04, _address, valveDriveAddress, payload, false));
        GD::out.printDebug("Handling WakeUp from Valve Drive: "+std::to_string(valveDriveAddress)+". Package to be sent: "+packet->hexString());
        std::shared_ptr<MAXCentral> central = std::dynamic_pointer_cast<MAXCentral>(valveDrive->getCentral());
        central->sendPacket(valveDrive->getPhysicalInterface(), packet);
    }
    catch (const std::exception &ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
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

void BcTcCWm::setMeasuredTemperature(float measuredTemperature, uint64_t &vdPeerID)
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
            GD::out.printInfo("New measured temperature for Valve drive with Peer ID" + std::to_string(vdPeerID) + "didn't change enough. Won't send it to save credits. Old: " + std::to_string(_measuredTemperature) + " New: " + std::to_string(_newMeasuredTemperature) + ".");
            return;
        }
        // temperature is float but needs to be converted to int. So we need to multiply it by 10
        saveVariable(1011, _newMeasuredTemperature);

        // next step: encode it, build packet and send it
        // get paired valve drive to find out the actual desired temperature
        std::shared_ptr<MAXPeer> valveDrive = std::dynamic_pointer_cast<MAXPeer>(getPeer(1, vdPeerID, 3));
        // get desired temperature from paired valve drive, we need to convert it from float to int
        // BaseLib::PVariable desiredTemperatureVariable = getValue(clientInfo, 1, "SET_TEMPERATURE", false, true); // how can I do this in a different way???
        // get desired Temperature from schedule
        float desiredTemperature = valveDrive->getTemperatureFromWeekplan();
        
        // now build and send the packet
        std::shared_ptr<MAXPacket> packet = buildFakePacket(measuredTemperature, desiredTemperature, valveDrive->getAddress(), valveDrive->getRXModes() & HomegearDevice::ReceiveModes::wakeOnRadio);
        GD::out.printDebug("Sending fake Wallthermostat packet from Peer: " + std::to_string(_peerID) + " to valve drive with Address: " + std::to_string(valveDrive->getAddress()) + "\nwith desired Temperature: " + std::to_string(desiredTemperature) + " \nand measured temperature: " + std::to_string(measuredTemperature)+"\n packet string is: "+packet->hexString());
        std::shared_ptr<MAXCentral> central = std::dynamic_pointer_cast<MAXCentral>(getCentral());
        central->sendPacket(valveDrive->getPhysicalInterface(), packet);
        _messageCounter++;
    }
    catch (const std::exception &ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

}