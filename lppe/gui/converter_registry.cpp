#include "gui.hpp"

#include <iostream>
#include <atomic>

static std::atomic<int> unique_id_counter{0};

VectorConverter::VectorConverter() {
    unique_id = ++unique_id_counter;
}

void VectorConverterRegistry::registerConverter(const std::string& name, Creator creator) {
    converter_names.push_back(name);
    creators.push_back(std::move(creator));
}

std::vector<std::string> VectorConverterRegistry::getConverterNames() const {
    return converter_names;
}

int VectorConverterRegistry::getConverterCount() const {
    return creators.size();
}

std::unique_ptr<VectorConverter> VectorConverterRegistry::createConverter(const int i) const {
    if (i < 0 || i >= creators.size()) {
        std::cerr << "createConverter: invalid index (" << i << ")" << std::endl;
        return nullptr;
    }
    return creators[i]();
}
