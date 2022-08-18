
#include "internal.hpp"

namespace CaDiCaL {

// Implementation of welfords online algorithm, copied from wikipedia:
// https://en.wikipedia.org/wiki/Algorithms_for_calculating_variance
void LBDAggregate::update(float new_value) {
    this->count += 1;
    float delta = new_value - this->mean;
    this->mean += delta / count;
    float delta2 = new_value - this->mean;
    this->m2 += delta * delta2;
}

float LBDAggregate::final_mean() {
    return this->mean;
}

float LBDAggregate::final_variance() {
    if (this->count == 1) {
        return 0.0;
    } else {
        return this->m2 / (this->count - 1);
    }
}

int LBDAggregate::final_count() {
    return this->count;
}

void LBDStats::update(std::vector<int> const& clause, int lbd_value) {
    std::vector<int> cls = clause;
    std::sort(cls.begin(), cls.end());
    auto it = this->data.find(cls);

    if (it != this->data.end()) {
        // in map
        it->second.update(lbd_value);
    } else {
        // Not in map
        this->data[cls] = {1, (float) lbd_value, 0};
    }
}

}