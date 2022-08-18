#ifndef _clause_lbd_stats_hpp_INCLUDED
#define _clause_lbd_stats_hpp_INCLUDED

#include<unordered_map>
#include<vector>

namespace CaDiCaL {

struct Clause;

struct LBDAggregate {
    public:
        int count;
        float mean;
        float m2;
    
    public:
        void update(float value);

        float final_mean();
        float final_variance();
        int final_count();
};

struct MyHash {
    std::size_t operator()(std::vector<int> const& s) const noexcept
    {
        std::size_t h = std::hash<std::size_t>{}(s.size());
        for (auto e : s) {
            std::size_t h1 = std::hash<int>{}(e);
            h = (h * h1 + 42257) ^ h1;
        }
        return h;
    }
};

struct LBDStats {
    public:
        std::unordered_map<std::vector<int>, LBDAggregate, MyHash> data;
    
    public:
        LBDStats() = default;
        ~LBDStats() = default;

        void update(std::vector<int> const& clause, int lbd_value);
};

}
#endif