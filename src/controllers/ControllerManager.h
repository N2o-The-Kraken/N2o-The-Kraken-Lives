#pragma once

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace kraken::controllers {

enum class ControllerType {
    AkaiAPC64,
    NumarkPartyMix,
    NativeInstrumentsX1
};

struct ControlMessage {
    std::string control;
    double value;
};

class Controller {
public:
    virtual ~Controller() = default;
    virtual ControllerType type() const noexcept = 0;
    void setCallback(std::function<void(const ControlMessage&)> callback) { m_callback = std::move(callback); }

    void emit(const ControlMessage& message) const {
        if (m_callback) {
            m_callback(message);
        }
    }

private:
    std::function<void(const ControlMessage&)> m_callback;
};

class ControllerManager {
public:
    using ControllerId = std::size_t;

    ControllerId addController(std::unique_ptr<Controller> controller);
    void removeController(ControllerId id);

    void routeMessage(ControllerId id, const ControlMessage& message) const;

    void onMessage(const std::function<void(ControllerType, const ControlMessage&)>& handler);

private:
    std::vector<std::unique_ptr<Controller>> m_controllers;
    std::function<void(ControllerType, const ControlMessage&)> m_handler;
};

class AkaiAPC64 : public Controller {
public:
    ControllerType type() const noexcept override { return ControllerType::AkaiAPC64; }
};

class NumarkPartyMix : public Controller {
public:
    ControllerType type() const noexcept override { return ControllerType::NumarkPartyMix; }
};

class NativeInstrumentsX1 : public Controller {
public:
    ControllerType type() const noexcept override { return ControllerType::NativeInstrumentsX1; }
};

} // namespace kraken::controllers
