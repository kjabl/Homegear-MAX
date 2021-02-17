#ifndef BCTCCWM_H
#define BCTCCWM_H

#include "../MAXPeer.h"

namespace MAX
{

class BcTcCWm : public MAXPeer
{
    public:
    BcTcCWm(uint32_t parentID, IPeerEventSink* eventHandler);
    BcTcCWm(uint32_t id, int32_t address, std::string serialNumber, uint32_t parentID, IPeerEventSink* eventHandler);
    virtual ~BcTcCWm();
    virtual void dispose();

    virtual bool isVirtual() {return true;}

    void setMeasuredTemperature(int32_t measuredTemperature);
    int32_t getNewMeasuredTemperature() {return _newMeasuredTemperature;}
    uint8_t encodeTemperature(int32_t temperature);

    protected:
    //In table variables
    int32_t _currentDutyCycleDeviceAddress = -1;
    int32_t _measuredTemperature = 0;
    int32_t _newMeasuredTemperature = 0;
    int64_t _lastDutyCycleEvent = 0;
    //end in table variables

    std::unordered_map<int32_t, bool> _decalcification;

    const int32_t _dutyCycleTimeOffset = 3000;
    std::atomic_bool _stopDutyCycleThread;
    std::thread _dutyCycleThread;
    int32_t _dutyCycleCounter = 0;
    std::thread _sendDutyCyclePacketThread;
    uint8_t _dutyCycleMessageCounter = 0;

    void init();
    void worker();
    virtual bool load(BaseLib::Systems::ICentral* device);
    virtual void loadVariables(BaseLib::Systems::ICentral* device, std::shared_ptr<BaseLib::Database::DataTable>& rows);
    virtual void saveVariables();

    void setDecalcification();  //TODO: wird ggf. nicht gebraucht, da Decalcification bereits so gesetzt werden kann?

    virtual PVariable putParamset(BaseLib::PRpcClientInfo clientInfo, int32_t channel, ParameterGroup::Type::Enum type, uint64_t remoteID, int32_t remoteChannel, PVariable variables, bool onlyPushing = false) {return Variable::createError(-32601, "Method not implemented by this virtual device.");}

    virtual PVariable setInterface(BaseLib::PRpcClientInfo clientInfo, std::string interfaceID) { return Variable::createError(-32601, "Method not implemented by this virtual device."); }

    virtual PVariable setValue(BaseLib::PRpcClientInfo clientInfo, uint32_t channel, std::string valueKey, PVariable value, bool wait) { return Variable::createError(-32601, "Method not implemented by this virtual device."); }
};
}
#endif