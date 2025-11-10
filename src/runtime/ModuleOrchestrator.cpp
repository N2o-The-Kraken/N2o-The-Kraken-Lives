#include "runtime/ModuleOrchestrator.h"

#include <iostream>

namespace kraken::runtime {

AIDebugger::AIDebugger(ai::AIAssistant& assistant) noexcept
    : m_assistant(assistant) {}

void AIDebugger::reportFailure(std::string_view moduleName, std::string_view errorMessage) const {
    ai::SessionEvent event{
        .trackId = std::string(moduleName),
        .action = "module_failure",
        .timestampSeconds = 0.0};
    m_assistant.recordEvent(event);

    if (!errorMessage.empty()) {
        std::clog << "[AI Debugger] " << moduleName << " reported an error: " << errorMessage << '\n';
    } else {
        std::clog << "[AI Debugger] " << moduleName << " reported a failure without diagnostics." << '\n';
    }
}

bool AIDebugger::attemptRecovery(std::string_view moduleName, std::string_view errorMessage) const {
    ai::SessionEvent event{
        .trackId = std::string(moduleName),
        .action = "module_recovery_attempt",
        .timestampSeconds = 0.0};
    m_assistant.recordEvent(event);

    std::clog << "[AI Debugger] Attempting automated recovery for " << moduleName << "..." << '\n';
    if (!errorMessage.empty()) {
        std::clog << "[AI Debugger] Last error: " << errorMessage << '\n';
    }

    if (!m_assistant.isTrainingActive()) {
        std::clog << "[AI Debugger] Assistant training paused; no automated fix available." << '\n';
        return false;
    }

    std::clog << "[AI Debugger] No offline repair model available; deferring to manual resolution." << '\n';
    return false;
}

ModuleOrchestrator::ModuleOrchestrator(AIDebugger& debugger) noexcept
    : m_debugger(debugger) {}

void ModuleOrchestrator::addModule(std::unique_ptr<Module> module) {
    m_modules.push_back(std::move(module));
}

bool ModuleOrchestrator::startAll() {
    if (m_started) {
        return true;
    }

    for (auto& module : m_modules) {
        std::string errorMessage;
        if (!module->start(&errorMessage)) {
            m_debugger.reportFailure(module->name(), errorMessage);
            if (m_debugger.attemptRecovery(module->name(), errorMessage)) {
                errorMessage.clear();
                if (module->start(&errorMessage)) {
                    continue;
                }
            }

            std::cerr << "Module startup aborted: " << module->name();
            if (!errorMessage.empty()) {
                std::cerr << " | " << errorMessage;
            }
            std::cerr << '\n';
            shutdownAll();
            return false;
        }
    }

    m_started = true;
    return true;
}

void ModuleOrchestrator::shutdownAll() {
    if (!m_started) {
        return;
    }

    for (auto it = m_modules.rbegin(); it != m_modules.rend(); ++it) {
        if (*it) {
            (*it)->shutdown();
        }
    }

    m_started = false;
}

} // namespace kraken::runtime

