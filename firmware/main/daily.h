#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool   valid;
    char   city[32];
    float  temp_c;
    float  feels_c;
    int    humidity;
    char   condition[16];
} daily_weather_t;

// Fetch daily.json over HTTPS and populate `out`. Returns true on success.
bool daily_fetch(daily_weather_t *out);

#ifdef __cplusplus
}
#endif
