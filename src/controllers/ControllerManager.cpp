#include "controllers/ControllerManager.h"

namespace kraken::controllers {

ControllerManager::ControllerId ControllerManager::addController(std::unique_ptr<Controller> controller) {
    controller->setCallback([this, type = controller->type()](const ControlMessage& message) {
        if (m_handler) {
            m_handler(type, message);
        }
    });
    m_controllers.push_back(std::move(controller));
    return m_controllers.size() - 1;
}

void ControllerManager::removeController(ControllerId id) {
    if (id >= m_controllers.size()) {
        return;
    }
    m_controllers.erase(m_controllers.begin() + static_cast<std::ptrdiff_t>(id));
}

void ControllerManager::routeMessage(ControllerId id, const ControlMessage& message) const {
    if (id >= m_controllers.size()) {
        return;
    }
    m_controllers[id]->emit(message);
}

void ControllerManager::onMessage(const std::function<void(ControllerType, const ControlMessage&)>& handler) {
    m_handler = handler;
}

void ControllerManager::emitVirtualMessage(ControllerType type, const ControlMessage& message) const {
    if (m_handler) {
        m_handler(type, message);
    }
}

} // namespace kraken::controllers
