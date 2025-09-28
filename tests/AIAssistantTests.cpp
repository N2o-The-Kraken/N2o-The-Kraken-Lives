#include "ai/AIAssistant.h"

#include <cassert>
#include <iostream>

int main() {
    kraken::ai::AIAssistant assistant;
    assistant.setMode(kraken::ai::AIMode::CollaborativeRecommend);
    const auto recs = assistant.recommendTracks("groove", 5);
    assert(!recs.empty());
    std::cout << "Received " << recs.size() << " groove recommendations\n";
    return 0;
}
