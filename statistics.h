//
// Created by RGAA on 2024-04-21.
//

#ifndef GAMMARAYPC_STATISTICS_H
#define GAMMARAYPC_STATISTICS_H

#include <vector>
#include <cstdint>
#include <string>

namespace tc
{

    constexpr auto kMaxStatCounts = 180;

    class Statistics {
    public:

        static Statistics* Instance() {
            static Statistics instance;
            return &instance;
        }

        void AppendDecodeDuration(uint32_t time);
        std::string AsProtoMessage();

    public:

        std::vector<uint32_t> decode_durations_;

    };

}

#endif //GAMMARAYPC_STATISTICS_H
