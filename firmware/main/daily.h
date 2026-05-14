#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    // Weather (current conditions)
    bool   has_weather;
    char   city[32];
    float  temp_c;
    float  feels_c;
    int    humidity;
    char   condition[16];

    // Closest asteroid this week
    bool   has_asteroids;
    int    asteroid_count;
    char   closest_name[24];
    int    closest_diameter_m;
    long   closest_miss_km;

    // APOD
    bool   has_apod;
    char   apod_title[96];
} daily_data_t;

// Fetch daily.json over HTTPS and populate `out`. Returns true if at
// least one of the sub-payloads (weather/asteroids/apod) parsed cleanly.
bool daily_fetch(daily_data_t *out);

#ifdef __cplusplus
}
#endif
