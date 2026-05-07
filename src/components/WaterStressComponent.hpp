#ifndef WATER_STRESS_COMPONENT_HPP
#define WATER_STRESS_COMPONENT_HPP

// Per-plant counter that accumulates while water uptake fails (the tile
// the plant sits on has no water in it) and ticks down every plant
// simulation tick. When it crosses MAX_WATER_STRESS_TICKS, the plant
// takes drought damage to its HealthComponent in `processPlants`. The
// existing HealthSystem then handles the kill on healthLevel <= 0.
struct WaterStressComponent {
  int water_stress_ticks = 0;

  // Increment per dry tick. Decrement on a wet tick is hardcoded to 1.
  // Keep this at 1 so a plant in 50/50 conditions hovers near zero
  // instead of slowly drifting toward death; raise it to make plants
  // less tolerant of intermittent dryness.
  static constexpr int STRESS_PER_DRY_TICK = 1;

  // Threshold the stress counter must exceed before drought damage
  // starts. At 1 stress/tick this is ~1000 ticks of continuous drought
  // before any HP is lost. Scenario factories and tests should override
  // via PhysicsSettings to compress for short-running scenarios.
  static constexpr int MAX_WATER_STRESS_TICKS = 1000;

  // Damage applied per stress *cycle* (not per tick). When the stress
  // counter crosses MAX_WATER_STRESS_TICKS, the plant takes this much
  // health damage and the counter resets to 0 — so the plant visibly
  // takes a hit, then has a full stress runway before the next hit.
  // With the defaults above, a 100-HP plant survives 5 cycles ≈
  // 5 × 2000 = 10000 ticks of pure drought.
  static constexpr int DROUGHT_DAMAGE_PER_CYCLE = 10;
};

#endif // WATER_STRESS_COMPONENT_HPP
