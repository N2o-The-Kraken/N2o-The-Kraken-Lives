#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "ai/AIAssistant.h"

namespace kraken::runtime {

class Module {
public:
    virtual ~Module() = default;
    virtual std::string name() const = 0;
    virtual bool start(std::string* errorMessage) = 0;
    virtual void shutdown() = 0;
};

class AIDebugger {
public:
    explicit AIDebugger(ai::AIAssistant& assistant) noexcept;

    void reportFailure(std::string_view moduleName, std::string_view errorMessage) const;
    bool attemptRecovery(std::string_view moduleName, std::string_view errorMessage) const;

private:
    ai::AIAssistant& m_assistant;
};

class ModuleOrchestrator {
public:
    explicit ModuleOrchestrator(AIDebugger& debugger) noexcept;

    void addModule(std::unique_ptr<Module> module);
    bool startAll();
    void shutdownAll();

private:
    AIDebugger& m_debugger;
    std::vector<std::unique_ptr<Module>> m_modules;
    bool m_started{false};
};

} // namespace kraken::runtime

