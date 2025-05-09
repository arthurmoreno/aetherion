#ifndef SUNINTENSITY_HPP
#define SUNINTENSITY_HPP

#include <cmath>
#include <cstdint>

#include "GameClock.hpp"

class SunIntensity {
   public:
    // Returns the sun intensity as a value between 0.0 and 1.0
    static float getIntensity(const GameClock& clock) {
        // Get current time components
        uint64_t hour = clock.getHour();
        uint64_t minute = clock.getMinute();
        uint64_t season = clock.getMonth();  // Seasons are 1 (Spring) to 4 (Winter)

        // Convert time to a decimal representation (e.g., 14.5 for 2:30 PM)
        float timeOfDay = hour + (minute / clock.getMinutesPerHour());

        // Define sunrise and sunset times based on the season
        float sunriseTime, sunsetTime;
        switch (season) {
            case 1:  // Spring
                sunriseTime = 6.0f;
                sunsetTime = 18.0f;
                break;
            case 2:  // Summer
                sunriseTime = 5.0f;
                sunsetTime = 19.0f;
                break;
            case 3:  // Fall
                sunriseTime = 7.0f;
                sunsetTime = 17.0f;
                break;
            case 4:  // Winter
                sunriseTime = 8.0f;
                sunsetTime = 16.0f;
                break;
            default:
                sunriseTime = 6.0f;
                sunsetTime = 18.0f;
                break;
        }

        // If the current time is outside of sunrise and sunset, it's night
        if (timeOfDay < sunriseTime || timeOfDay > sunsetTime) {
            return 0.0f;
        }

        // Calculate the proportion of the day that has passed since sunrise
        float dayProgress = (timeOfDay - sunriseTime) / (sunsetTime - sunriseTime);

        // Use a sine wave to simulate the sun's intensity throughout the day
        float intensity = std::sin(dayProgress * M_PI);  // M_PI is 180 degrees in radians

        // Ensure the intensity is within the valid range [0.0, 1.0]
        intensity = std::max(0.0f, std::min(intensity, 1.0f));

        return intensity;
    }
};

#endif  // SUNINTENSITY_HPP
