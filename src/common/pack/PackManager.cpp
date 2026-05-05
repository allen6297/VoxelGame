#include "pack/PackManager.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_set>

#include "data/JsonValue.hpp"

namespace voxel {

// ── Discovery ─────────────────────────────────────────────────────────────────

void PackManager::discover(const std::filesystem::path& packsDir) {
    packsDir_ = packsDir;
    packs_.clear();

    if (!std::filesystem::exists(packsDir)) {
        std::cerr << "PackManager: packs directory not found: " << packsDir << "\n";
        return;
    }

    for (const auto& entry : std::filesystem::directory_iterator(packsDir)) {
        const auto& p = entry.path();
        const bool isFolder = entry.is_directory();
        const bool isZip    = entry.is_regular_file() && p.extension() == ".zip";

        if (!isFolder && !isZip) continue;

        try {
            packs_.emplace_back(p);
        } catch (const std::exception& e) {
            std::cerr << "PackManager: failed to load pack '" << p.stem() << "': " << e.what() << "\n";
        }
    }

    const auto order = loadOrder();
    applyOrder(order);
    saveOrder(buildOrder());

    std::cout << "PackManager: loaded " << packs_.size() << " pack(s)\n";
    for (const auto& pack : packs_) {
        std::cout << "  [" << pack.id() << "] " << pack.manifest().name
                  << " v" << pack.manifest().version << "\n";
    }
}

// ── Order ─────────────────────────────────────────────────────────────────────

std::vector<std::string> PackManager::loadOrder() const {
    const auto orderFile = packsDir_.parent_path() / "pack_order.json";
    if (!std::filesystem::exists(orderFile)) return {};

    std::ifstream stream(orderFile);
    if (!stream) return {};

    std::ostringstream buf;
    buf << stream.rdbuf();

    try {
        const JsonValue root = parseJson(buf.str());
        if (!root.isObject()) return {};

        const auto& obj = root.asObject();
        const auto  it  = obj.find("order");
        if (it == obj.end() || !it->second.isArray()) return {};

        std::vector<std::string> order;
        for (const auto& v : it->second.asArray()) {
            if (v.isString()) order.push_back(v.asString());
        }
        return order;
    } catch (...) {
        return {};
    }
}

std::vector<std::string> PackManager::buildOrder() const {
    std::vector<std::string> order;
    for (const auto& pack : packs_) order.push_back(pack.id());
    return order;
}

void PackManager::saveOrder(const std::vector<std::string>& order) const {
    const auto orderFile = packsDir_.parent_path() / "pack_order.json";
    std::ofstream stream(orderFile);
    if (!stream) {
        std::cerr << "PackManager: could not write pack_order.json\n";
        return;
    }

    stream << "{\n  \"order\": [";
    for (std::size_t i = 0; i < order.size(); ++i) {
        stream << "\n    \"" << order[i] << "\"";
        if (i + 1 < order.size()) stream << ",";
    }
    stream << "\n  ]\n}\n";
}

void PackManager::applyOrder(const std::vector<std::string>& order) {
    // Build an index: id → position in order list (lower = higher priority)
    std::unordered_map<std::string, std::size_t> priority;
    for (std::size_t i = 0; i < order.size(); ++i) priority[order[i]] = i;

    // Packs not in the order file get inserted just above "base"
    const std::size_t basePriority  = priority.count("base") ? priority["base"] : order.size();
    std::size_t       nextNewPriority = basePriority;  // new packs slot in just above base

    for (auto& pack : packs_) {
        if (!priority.count(pack.id())) {
            priority[pack.id()] = nextNewPriority++;
        }
    }

    // "base" always last
    for (auto& pack : packs_) {
        if (pack.id() == "base") priority[pack.id()] = std::numeric_limits<std::size_t>::max();
    }

    std::stable_sort(packs_.begin(), packs_.end(), [&](const Pack& a, const Pack& b) {
        return priority[a.id()] < priority[b.id()];
    });
}

// ── Queries ───────────────────────────────────────────────────────────────────

const Pack* PackManager::findPack(const std::string& id) const {
    for (const auto& pack : packs_) {
        if (pack.id() == id) return &pack;
    }
    return nullptr;
}

std::optional<std::string> PackManager::readFile(const std::string& relativePath) const {
    for (const auto& pack : packs_) {
        auto content = pack.readFile(relativePath);
        if (content) return content;
    }
    return std::nullopt;
}

std::vector<std::string> PackManager::listFiles(const std::string& subdir) const {
    std::vector<std::string>   result;
    std::unordered_set<std::string> seen;

    for (const auto& pack : packs_) {
        for (auto& file : pack.listFiles(subdir)) {
            if (seen.insert(file).second) result.push_back(std::move(file));
        }
    }
    return result;
}

}  // namespace voxel
