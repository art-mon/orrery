#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool   valid;
    char   condition[16];
    float  temp_min_c;
    float  temp_max_c;
} daily_forecast_t;

typedef struct {
    // Weather (current conditions)
    bool   has_weather;
    char   city[32];
    float  temp_c;
    float  feels_c;
    int    humidity;
    float  wind_kmh;
    char   condition[16];

    // 3-day forecast columns: 0=today, 1=tomorrow, 2=day-after
    daily_forecast_t fc[3];

    // Earth events (EONET + USGS quakes merged) — up to 10
    int event_count;
    struct {
        char  title[48];
        char  category[16];
        float lon;
        float lat;
    } events[10];

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
