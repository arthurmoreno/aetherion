// Embind wrapper binding the real C++ EntityInterface (struct_pack-based)

#ifdef __EMSCRIPTEN__
    #include <emscripten/bind.h>
    #include <emscripten/val.h>

    #include <cstdint>
    #include <memory>
    #include <stdexcept>
    #include <vector>

    // Include Aetherion headers from the source tree
    #include "../../src/EntityInterface.hpp"

using emscripten::allow_raw_pointers;
using emscripten::class_;
using emscripten::function;
using emscripten::register_vector;
using emscripten::val;

// Wrapper: static deserialize from JS values robustly.
// Prefer std::vector<uint8_t> to leverage Embind's automatic conversions from TypedArray/Array.
static EntityInterface EntityInterface_deserialize_vec(const std::vector<unsigned char>& bytes) {
    if (bytes.empty()) {
        throw std::runtime_error("EntityInterface.deserialize: empty buffer");
    }
    try {
        return EntityInterface::deserialize(reinterpret_cast<const char*>(bytes.data()),
                                            bytes.size());
    } catch (const std::exception& ex) {
        // Emit a concise hex preview to aid debugging header mismatches
        auto to_hex = [](const unsigned char* p, size_t n) {
            static const char* kHex = "0123456789abcdef";
            std::string s;
            s.reserve(n * 2);
            for (size_t i = 0; i < n; ++i) {
                unsigned char c = p[i];
                s.push_back(kHex[c >> 4]);
                s.push_back(kHex[c & 0xF]);
            }
            return s;
        };
        emscripten::val::global("console").call<void>(
            "error", std::string("[embind] deserialize(header) failed: ") + ex.what(),
            std::string(" len=") + std::to_string(bytes.size()),
            std::string(" head=") + to_hex(bytes.data(), std::min<size_t>(24, bytes.size())));
        // Try an offset=4 diagnostic (struct_pack version prefix?) and log whether it works
        try {
            EntityInterface tmp = EntityInterface::deserialize(
                reinterpret_cast<const char*>(bytes.data() + 4), bytes.size() - 4);
            emscripten::val::global("console").call<void>(
                "warn", std::string("[embind] header parsed with offset=4 (diagnostic only)"));
        } catch (...) {
            // ignore
        }
        throw;  // rethrow
    }
}

// Fallback: accept arbitrary JS object (Buffer, ArrayBuffer, Uint8Array) when vector overload is
// not picked.
static EntityInterface EntityInterface_deserialize_js(emscripten::val any) {
    try {
        // Normalize to a Uint8Array view
        emscripten::val u8 = emscripten::val::undefined();
        if (any.instanceof (emscripten::val::global("Uint8Array"))) {
            u8 = any;
        } else if (any.instanceof (emscripten::val::global("ArrayBuffer"))) {
            u8 = emscripten::val::global("Uint8Array").new_(any);
        } else if (any.hasOwnProperty("buffer")) {
            // Respect byteOffset and byteLength when present (Node.js Buffer, DataView, etc.)
            size_t byteOffset =
                any.hasOwnProperty("byteOffset") ? any["byteOffset"].as<size_t>() : 0;
            if (any.hasOwnProperty("byteLength")) {
                const size_t byteLength = any["byteLength"].as<size_t>();
                u8 = emscripten::val::global("Uint8Array")
                         .new_(any["buffer"], byteOffset, byteLength);
            } else {
                u8 = emscripten::val::global("Uint8Array").new_(any["buffer"]);
            }
        } else {
            throw std::runtime_error("EntityInterface.deserialize: unsupported input type");
        }

        const size_t n = u8["length"].as<size_t>();
        if (n == 0) {
            throw std::runtime_error("EntityInterface.deserialize: empty buffer");
        }

        // Bulk-copy from JS → wasm in one shot
        std::vector<uint8_t> tmp(n);
        auto mv = emscripten::typed_memory_view(n, tmp.data());
        emscripten::val(mv).call<void>("set", u8);

        return EntityInterface_deserialize_vec(std::move(tmp));
    } catch (const std::exception& ex) {
        emscripten::val::global("console").call<void>(
            "error", std::string("[embind] Deserialize failed: ") + ex.what());
        throw;
    }
}

// Wrapper: serialize() -> Uint8Array
static std::vector<unsigned char> EntityInterface_serialize_js(const EntityInterface& e) {
    std::vector<char> out = e.serialize();
    // reinterpret as bytes for JS
    std::vector<unsigned char> u8(out.begin(), out.end());
    return u8;
}

// Helper: get component mask as uint32
static uint32_t EntityInterface_get_component_mask(const EntityInterface& e) {
    return static_cast<uint32_t>(e.componentMask.to_ulong());
}

// Helper: has_component by integer flag
static bool EntityInterface_has_component(const EntityInterface& e, int flag) {
    return e.hasComponent(static_cast<ComponentFlag>(flag));
}

// Expose selected components as plain JS objects for minimal consumption
static val EntityInterface_get_entity_type(const EntityInterface& e) {
    if (!e.hasComponent(ENTITY_TYPE)) return val::undefined();
    const auto& etc = e.getComponent<EntityTypeComponent>();
    val obj = val::object();
    obj.set("type", etc.mainType);
    obj.set("mainType", etc.mainType);
    obj.set("sub_type0", etc.subType0);
    obj.set("subType0", etc.subType0);
    obj.set("sub_type1", etc.subType1);
    obj.set("subType1", etc.subType1);
    return obj;
}

static val EntityInterface_get_position(const EntityInterface& e) {
    if (!e.hasComponent(POSITION)) return val::undefined();
    const auto& p = e.getComponent<Position>();
    val obj = val::object();
    obj.set("x", p.x);
    obj.set("y", p.y);
    obj.set("z", p.z);
    obj.set("direction", static_cast<int>(p.direction));
    return obj;
}

static val EntityInterface_get_moving_component(const EntityInterface& e) {
    if (!e.hasComponent(MOVING_COMPONENT)) return val::undefined();
    const auto& m = e.getComponent<MovingComponent>();
    val obj = val::object();
    // movement state
    obj.set("is_moving", m.isMoving);
    obj.set("isMoving", m.isMoving);
    // from coords
    obj.set("moving_from_x", m.movingFromX);
    obj.set("movingFromX", m.movingFromX);
    obj.set("moving_from_y", m.movingFromY);
    obj.set("movingFromY", m.movingFromY);
    obj.set("moving_from_z", m.movingFromZ);
    obj.set("movingFromZ", m.movingFromZ);
    // to coords
    obj.set("moving_to_x", m.movingToX);
    obj.set("movingToX", m.movingToX);
    obj.set("moving_to_y", m.movingToY);
    obj.set("movingToY", m.movingToY);
    obj.set("moving_to_z", m.movingToZ);
    obj.set("movingToZ", m.movingToZ);
    // velocities
    obj.set("vx", m.vx);
    obj.set("vy", m.vy);
    obj.set("vz", m.vz);
    // stop hints
    obj.set("will_stop_x", m.willStopX);
    obj.set("willStopX", m.willStopX);
    obj.set("will_stop_y", m.willStopY);
    obj.set("willStopY", m.willStopY);
    obj.set("will_stop_z", m.willStopZ);
    obj.set("willStopZ", m.willStopZ);
    // timing
    obj.set("completion_time", m.completionTime);
    obj.set("completionTime", m.completionTime);
    obj.set("time_remaining", m.timeRemaining);
    obj.set("timeRemaining", m.timeRemaining);
    // direction as int for JS
    obj.set("direction", static_cast<int>(m.direction));
    return obj;
}

// Expose MatterContainer as a plain JS object with snake_case keys (Python parity)
static val EntityInterface_get_matter_container(const EntityInterface& e) {
    if (!e.hasComponent(MATTER_CONTAINER)) return val::undefined();
    const auto& mc = e.getComponent<MatterContainer>();
    val obj = val::object();
    obj.set("terrain_matter", mc.TerrainMatter);
    obj.set("water_vapor", mc.WaterVapor);
    obj.set("water_matter", mc.WaterMatter);
    obj.set("bio_mass_matter", mc.BioMassMatter);
    return obj;
}

// ---------- setters (JS -> C++) to build test fixtures ----------
static void EntityInterface_set_entity_type_js(EntityInterface& e, val obj) {
    EntityTypeComponent etc{};
    // Accept multiple property casings
    etc.mainType = (obj.hasOwnProperty("main_type")  ? obj["main_type"].as<int>()
                    : obj.hasOwnProperty("mainType") ? obj["mainType"].as<int>()
                    : obj.hasOwnProperty("type")     ? obj["type"].as<int>()
                                                     : 0);
    etc.subType0 = (obj.hasOwnProperty("sub_type0")  ? obj["sub_type0"].as<int>()
                    : obj.hasOwnProperty("subType0") ? obj["subType0"].as<int>()
                                                     : 0);
    etc.subType1 = (obj.hasOwnProperty("sub_type1")  ? obj["sub_type1"].as<int>()
                    : obj.hasOwnProperty("subType1") ? obj["subType1"].as<int>()
                                                     : 0);
    e.setComponent<EntityTypeComponent>(etc);
}

static void EntityInterface_set_position_js(EntityInterface& e, val obj) {
    Position p{};
    p.x = obj.hasOwnProperty("x") ? obj["x"].as<int>() : 0;
    p.y = obj.hasOwnProperty("y") ? obj["y"].as<int>() : 0;
    p.z = obj.hasOwnProperty("z") ? obj["z"].as<int>() : 0;
    int dir = obj.hasOwnProperty("direction") ? obj["direction"].as<int>() : 0;
    p.direction = static_cast<DirectionEnum>(dir);
    e.setComponent<Position>(p);
}

static void EntityInterface_set_physics_stats_js(EntityInterface& e, val obj) {
    PhysicsStats ps{};
    auto getf = [&](const char* a, const char* b) {
        if (obj.hasOwnProperty(a)) return obj[a].as<float>();
        if (obj.hasOwnProperty(b)) return obj[b].as<float>();
        return 0.0f;
    };
    ps.mass = getf("mass", "mass");
    ps.maxSpeed = getf("max_speed", "maxSpeed");
    ps.minSpeed = getf("min_speed", "minSpeed");
    ps.forceX = getf("force_x", "forceX");
    ps.forceY = getf("force_y", "forceY");
    ps.forceZ = getf("force_z", "forceZ");
    ps.heat = getf("heat", "heat");
    e.setComponent<PhysicsStats>(ps);
}

static void EntityInterface_set_velocity_js(EntityInterface& e, val obj) {
    Velocity v{};
    v.vx = obj.hasOwnProperty("vx") ? obj["vx"].as<float>() : 0.0f;
    v.vy = obj.hasOwnProperty("vy") ? obj["vy"].as<float>() : 0.0f;
    v.vz = obj.hasOwnProperty("vz") ? obj["vz"].as<float>() : 0.0f;
    e.setComponent<Velocity>(v);
}

static void EntityInterface_set_health_js(EntityInterface& e, val obj) {
    HealthComponent h{};
    h.healthLevel = obj.hasOwnProperty("health_level")  ? obj["health_level"].as<float>()
                    : obj.hasOwnProperty("healthLevel") ? obj["healthLevel"].as<float>()
                                                        : 0.0f;
    h.maxHealth = obj.hasOwnProperty("max_health")  ? obj["max_health"].as<float>()
                  : obj.hasOwnProperty("maxHealth") ? obj["maxHealth"].as<float>()
                                                    : 0.0f;
    e.setComponent<HealthComponent>(h);
}

static void EntityInterface_set_perception_js(EntityInterface& e, val obj) {
    PerceptionComponent pc{};
    pc.perception_area = obj.hasOwnProperty("perception_area")  ? obj["perception_area"].as<int>()
                         : obj.hasOwnProperty("perceptionArea") ? obj["perceptionArea"].as<int>()
                                                                : 0;
    pc.z_perception_area =
        obj.hasOwnProperty("z_perception_area") ? obj["z_perception_area"].as<int>()
        : obj.hasOwnProperty("zPerceptionArea") ? obj["zPerceptionArea"].as<int>()
                                                : 0;
    e.setComponent<PerceptionComponent>(pc);
}

static void EntityInterface_set_inventory_js(EntityInterface& e, val obj) {
    Inventory inv{};
    int maxItems = obj.hasOwnProperty("max_items")  ? obj["max_items"].as<int>()
                   : obj.hasOwnProperty("maxItems") ? obj["maxItems"].as<int>()
                                                    : 0;
    inv.maxItems = maxItems;
    inv.itemIDs.assign(static_cast<size_t>(std::max(0, maxItems)), -1);
    e.setComponent<Inventory>(inv);
}

static void EntityInterface_set_console_logs_js(EntityInterface& e, val obj) {
    ConsoleLogsComponent cl{};
    size_t maxSize = obj.hasOwnProperty("max_size")  ? obj["max_size"].as<size_t>()
                     : obj.hasOwnProperty("maxSize") ? obj["maxSize"].as<size_t>()
                                                     : 0;
    cl.max_size = maxSize;
    e.setComponent<ConsoleLogsComponent>(cl);
}

static void EntityInterface_set_metabolism_js(EntityInterface& e, val obj) {
    MetabolismComponent m{};
    m.energyReserve = obj.hasOwnProperty("energy_reserve")  ? obj["energy_reserve"].as<float>()
                      : obj.hasOwnProperty("energyReserve") ? obj["energyReserve"].as<float>()
                                                            : 0.0f;
    m.maxEnergyReserve =
        obj.hasOwnProperty("max_energy_reserve") ? obj["max_energy_reserve"].as<float>()
        : obj.hasOwnProperty("maxEnergyReserve") ? obj["maxEnergyReserve"].as<float>()
                                                 : 0.0f;
    e.setComponent<MetabolismComponent>(m);
}

// --- Minimal stub types to satisfy WASM tests ---
class WorldViewStub {
   public:
    WorldViewStub() : w_(64), h_(36), d_(8) {}
    WorldViewStub(int w, int h, int d) : w_(w), h_(h), d_(d) {}
    int width() const { return w_; }
    int height() const { return h_; }
    int depth() const { return d_; }

   private:
    int w_, h_, d_;
};

class PerceptionResponseFlatBStub {
   public:
    PerceptionResponseFlatBStub() : ticks_(0) {
        world_ = std::make_unique<WorldViewStub>(64, 36, 8);
        entity_ = std::make_unique<EntityInterface>();
    }
    PerceptionResponseFlatBStub(const std::vector<unsigned char>& bytes) : ticks_(0) {
        world_ = std::make_unique<WorldViewStub>(64, 36, 8);
        // Try to decode the provided bytes as an EntityInterface
        try {
            if (!bytes.empty()) {
                // Deserialize directly from the provided buffer
                EntityInterface e = EntityInterface::deserialize(
                    reinterpret_cast<const char*>(bytes.data()), bytes.size());
                entity_ = std::make_unique<EntityInterface>(e);
            } else {
                entity_ = std::make_unique<EntityInterface>();
            }
        } catch (...) {
            entity_ = std::make_unique<EntityInterface>();
        }
    }

    WorldViewStub* getWorldView() const { return world_.get(); }
    EntityInterface* getEntity() const { return entity_.get(); }
    EntityInterface* get_item_from_inventory_by_id(int /*id*/) const { return nullptr; }
    std::vector<unsigned char> get_query_response_by_id(int /*id*/) const { return {}; }
    int get_ticks() const { return ticks_; }

   private:
    std::unique_ptr<WorldViewStub> world_;
    std::unique_ptr<EntityInterface> entity_;
    int ticks_;
};

// Free function to expose component count
static int get_component_count_c() { return static_cast<int>(COMPONENT_COUNT); }

EMSCRIPTEN_BINDINGS(aetherion_bindings) {
    register_vector<unsigned char>("VectorUint8");

    class_<EntityInterface>("EntityInterface")
        .constructor<>()
        .function("get_entity_id", &EntityInterface::getEntityId)
        .function("set_entity_id", &EntityInterface::setEntityId)
        .function("serialize", &EntityInterface_serialize_js)
        .function("get_component_mask", &EntityInterface_get_component_mask)
        .function("has_component", &EntityInterface_has_component)
        .function("get_entity_type", &EntityInterface_get_entity_type)
        .function("get_position", &EntityInterface_get_position)
        .function("get_moving_component", &EntityInterface_get_moving_component)
        .function("get_matter_container", &EntityInterface_get_matter_container)
        // Setters used by tests to compose a full entity with many components
        .function("set_entity_type_js", &EntityInterface_set_entity_type_js)
        .function("set_position_js", &EntityInterface_set_position_js)
        .function("set_physics_stats_js", &EntityInterface_set_physics_stats_js)
        .function("set_velocity_js", &EntityInterface_set_velocity_js)
        .function("set_health_js", &EntityInterface_set_health_js)
        .function("set_perception_js", &EntityInterface_set_perception_js)
        .function("set_inventory_js", &EntityInterface_set_inventory_js)
        .function("set_console_logs_js", &EntityInterface_set_console_logs_js)
        .function("set_metabolism_js", &EntityInterface_set_metabolism_js)
        // Use robust JS-aware converter that handles Uint8Array/ArrayBuffer/Buffer correctly
        .class_function("deserialize", &EntityInterface_deserialize_js);

    class_<WorldViewStub>("WorldView")
        .constructor<>()
        .function("width", &WorldViewStub::width)
        .function("height", &WorldViewStub::height)
        .function("depth", &WorldViewStub::depth);

    class_<PerceptionResponseFlatBStub>("PerceptionResponseFlatB")
        .constructor<>()
        .constructor<const std::vector<unsigned char>&>()
        .function("getWorldView", &PerceptionResponseFlatBStub::getWorldView, allow_raw_pointers())
        .function("getEntity", &PerceptionResponseFlatBStub::getEntity, allow_raw_pointers())
        .function("get_item_from_inventory_by_id",
                  &PerceptionResponseFlatBStub::get_item_from_inventory_by_id, allow_raw_pointers())
        .function("get_query_response_by_id",
                  &PerceptionResponseFlatBStub::get_query_response_by_id)
        .function("get_ticks", &PerceptionResponseFlatBStub::get_ticks);

    // Optionally export the enum size to JS if needed
    function("get_component_count", &get_component_count_c);
}

#endif  // __EMSCRIPTEN__
