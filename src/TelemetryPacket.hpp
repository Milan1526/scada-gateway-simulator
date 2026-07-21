#pragma once  // spreceno ucitavanje ovog fajla vise puta kako se kompajler ne bi zbunio
#include <cstdint> // Biblioteka koja nam daje fiksne tipove podataka (uint32_t, uint64_t)

// Ukljucujemo striktno pakovanje memorije (1 bajt poravnanje)
#pragma pack(push, 1)
struct TelemetryPacket {
    uint32_t station_id;
    uint64_t timestamp;
    float voltage;
    float current;
    float frequency;
};

// Vracamo podesavanja memorije u normalu
#pragma pack(pop)