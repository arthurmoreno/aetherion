#ifndef GAMECLOCK_HPP
#define GAMECLOCK_HPP

#include <cstdint>  // For uint64_t
#include <string>

class GameClock {
   private:
    uint64_t ticks;  // Total seconds elapsed in the game

    // Constants for time calculations
    static const uint64_t SECONDS_PER_MINUTE = 10;
    // static const uint64_t MINUTES_PER_HOUR = 60;
    static const uint64_t MINUTES_PER_HOUR = 10;  // Remove this one, just used for faster debugging
    static const uint64_t HOURS_PER_DAY = 24;
    static const uint64_t DAYS_PER_MONTH = 28;  // Also represents the days in a season
    static const uint64_t MONTHS_PER_YEAR = 4;  // Number of seasons

   public:
    // Constructors
    GameClock() : ticks(0) {}

    GameClock(uint64_t initialTicks) : ticks(initialTicks) {}

    // Advances the clock by one tick (one in-game second)
    void tick() { ticks++; }

    // Sets the ticks value
    void setTicks(uint64_t newTicks) { ticks = newTicks; }

    // Gets the ticks value
    uint64_t getTicks() const { return ticks; }

    // Returns the total number of ticks (seconds) elapsed
    uint64_t getSeconds() const { return ticks; }

    // Returns the current second within the minute
    uint64_t getSecond() const { return ticks % SECONDS_PER_MINUTE; }

    // Returns the current minute within the hour
    uint64_t getMinute() const { return (ticks / SECONDS_PER_MINUTE) % MINUTES_PER_HOUR; }

    uint64_t getMinutesPerHour() const { return MINUTES_PER_HOUR; }

    // Returns the current hour within the day
    uint64_t getHour() const {
        return (ticks / (SECONDS_PER_MINUTE * MINUTES_PER_HOUR)) % HOURS_PER_DAY;
    }

    // Returns the current day within the month (1-28)
    uint64_t getDay() const {
        return (ticks / (SECONDS_PER_MINUTE * MINUTES_PER_HOUR * HOURS_PER_DAY)) % DAYS_PER_MONTH +
               1;
    }

    // Returns the current month (season) within the year (1-4)
    uint64_t getMonth() const {
        return (ticks / (SECONDS_PER_MINUTE * MINUTES_PER_HOUR * HOURS_PER_DAY * DAYS_PER_MONTH)) %
                   MONTHS_PER_YEAR +
               1;
    }

    // Returns the current year
    uint64_t getYear() const {
        return ticks / (SECONDS_PER_MINUTE * MINUTES_PER_HOUR * HOURS_PER_DAY * DAYS_PER_MONTH *
                        MONTHS_PER_YEAR) +
               1;
    }

    // Returns the current season as a string
    std::string getSeason() const {
        switch (getMonth()) {
            case 1:
                return "Spring";
            case 2:
                return "Summer";
            case 3:
                return "Fall";
            case 4:
                return "Winter";
            default:
                return "Unknown";
        }
    }
};

#endif  // GAMECLOCK_HPP