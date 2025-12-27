#include "gui.hpp"

#include <iostream>
#include <atomic>

static std::atomic<int> unique_id_counter{0};

Filter::Filter() {
    unique_id = ++unique_id_counter;
}

void FilterRegistry::registerFilter(const std::string& name, Creator creator) {
    filter_names.push_back(name);
    creators.push_back(std::move(creator));
}

std::vector<std::string> FilterRegistry::getFilterNames() const {
    return filter_names;
}

int FilterRegistry::getFilterCount() const {
    return creators.size();
}

std::unique_ptr<Filter> FilterRegistry::createFilter(const int i) const {
    if (i < 0 || i >= creators.size()) {
        std::cerr << "createFilter: invalid index (" << i << ")" << std::endl;
        return nullptr;
    }
    return creators[i]();
}
