#ifndef PYREGISTRY_HPP
#define PYREGISTRY_HPP

#include <nanobind/nanobind.h>

#include <entt/entt.hpp>
#include <string>
#include <unordered_map>
#include <vector>

#include "components/HealthComponents.hpp"
#include "components/ItemsComponents.hpp"
#include "components/MetabolismComponents.hpp"
#include "components/MovingComponent.hpp"
#include "components/PhysicsComponents.hpp"

namespace nb = nanobind;
using namespace entt::literals;

class PyRegistry {
   public:
    // Constructor that accepts a reference to an existing registry
    PyRegistry(entt::registry& reg, entt::dispatcher& disp) : registry(reg), dispatcher(disp) {}

    // Delete copy constructor and copy assignment operator
    PyRegistry(const PyRegistry&) = delete;
    PyRegistry& operator=(const PyRegistry&) = delete;

    // Delete move constructor and move assignment operator
    PyRegistry(PyRegistry&&) = delete;
    PyRegistry& operator=(PyRegistry&&) = delete;

    // Entity management
    int create_entity() {
        auto entity = registry.create();
        return entt::to_integral(entity);
    }

    void destroy_entity(entt::entity entity) { registry.destroy(entity); }

    // View entities with specific components
    nb::list view(const std::vector<std::string>& component_names) {
        nb::list result;

        // Map of component names to type IDs
        static const std::unordered_map<std::string, entt::id_type> component_type_map = {
            {"Position", entt::type_hash<Position>::value()},
            {"Velocity", entt::type_hash<Velocity>::value()},
            {"MovingComponent", entt::type_hash<MovingComponent>::value()},
            {"MetabolismComponent", entt::type_hash<MetabolismComponent>::value()},
            {"DigestionComponent", entt::type_hash<DigestionComponent>::value()},
            {"FoodItem", entt::type_hash<FoodItem>::value()},
            {"EntityTypeComponent", entt::type_hash<EntityTypeComponent>::value()},
            {"WeaponAttributes", entt::type_hash<WeaponAttributes>::value()},
            {"Durability", entt::type_hash<Durability>::value()},
            {"MeeleAttackComponent", entt::type_hash<MeeleAttackComponent>::value()},
            {"ConsoleLogsComponent", entt::type_hash<ConsoleLogsComponent>::value()}};

        // Create a runtime_view
        entt::runtime_view view;

        // Iterate over requested components and add their storages to the runtime_view
        for (const auto& name : component_names) {
            if (auto it = component_type_map.find(name); it != component_type_map.end()) {
                if (auto* storage = registry.storage(it->second); storage) {
                    view.iterate(*storage);
                }
            }
        }

        // If no storages were added to the view, return an empty result
        if (view.begin() == view.end()) {
            return result;
        }

        // Iterate the runtime_view and collect entities
        for (auto entity : view) {
            result.append(static_cast<std::uint32_t>(entity));
        }

        return result;
    }

    // Get a component from an entity
    template <typename Component>
    bool all_of_single(entt::entity entity) {
        return registry.all_of<Component>(entity);
    }

    bool has_all_components(uint32_t entity_id, const std::vector<std::string>& component_names) {
        entt::entity entity = entt::entity{entity_id};
        // Map of component names to type IDs
        static const std::unordered_map<std::string, entt::id_type> component_type_map = {
            {"Position", entt::type_hash<Position>::value()},
            {"Velocity", entt::type_hash<Velocity>::value()},
            {"MovingComponent", entt::type_hash<MovingComponent>::value()},
            {"MetabolismComponent", entt::type_hash<MetabolismComponent>::value()},
            {"DigestionComponent", entt::type_hash<DigestionComponent>::value()},
            {"FoodItem", entt::type_hash<FoodItem>::value()},
            {"EntityTypeComponent", entt::type_hash<EntityTypeComponent>::value()},
            {"WeaponAttributes", entt::type_hash<WeaponAttributes>::value()},
            {"Durability", entt::type_hash<Durability>::value()},
            {"MeeleAttackComponent", entt::type_hash<MeeleAttackComponent>::value()},
            {"ConsoleLogsComponent", entt::type_hash<ConsoleLogsComponent>::value()},
            {"Inventory", entt::type_hash<Inventory>::value()}};

        // Iterate over requested components and check their storages
        for (const auto& name : component_names) {
            if (auto it = component_type_map.find(name); it != component_type_map.end()) {
                if (auto* storage = registry.storage(it->second); storage) {
                    // Check if the entity exists in the storage
                    if (!storage->contains(entity)) {
                        return false;  // Entity does not have this component
                    }
                } else {
                    return false;  // Storage for this component does not exist
                }
            } else {
                return false;
            }
        }

        return true;
    }

    // Get a component from an entity
    template <typename Component>
    Component* get(entt::entity entity) {
        if (registry.all_of<Component>(entity)) {
            return &registry.get<Component>(entity);
        }
        return nullptr;
    }

    // Set a component on an entity
    template <typename Component>
    void set(entt::entity entity, const Component& component) {
        registry.emplace_or_replace<Component>(entity, component);
    }

    // Remove a component from an entity
    template <typename Component>
    void remove(entt::entity entity) {
        registry.remove<Component>(entity);
    }

    // Get a component from an entity
    nb::object get_component(uint32_t entity_id, const std::string& component_name) {
        entt::entity entity = entt::entity{entity_id};
        if (component_name == "Position") {
            Position* comp = get<Position>(entity);
            if (comp) return nb::cast(*comp);
        }
        if (component_name == "Velocity") {
            Velocity* comp = get<Velocity>(entity);
            if (comp) return nb::cast(*comp);
        }
        if (component_name == "MovingComponent") {
            MovingComponent* comp = get<MovingComponent>(entity);
            if (comp) return nb::cast(*comp);
        }
        if (component_name == "Inventory") {
            Inventory* comp = get<Inventory>(entity);
            if (comp) return nb::cast(*comp);
        }
        if (component_name == "MetabolismComponent") {
            MetabolismComponent* comp = get<MetabolismComponent>(entity);
            if (comp) return nb::cast(*comp);
        }
        if (component_name == "DigestionComponent") {
            DigestionComponent* comp = get<DigestionComponent>(entity);
            if (comp) return nb::cast(*comp);
        }
        if (component_name == "FoodItem") {
            FoodItem* comp = get<FoodItem>(entity);
            if (comp) return nb::cast(*comp);
        }
        if (component_name == "HealthComponent") {
            HealthComponent* comp = get<HealthComponent>(entity);
            if (comp) return nb::cast(*comp);
        }
        if (component_name == "EntityTypeComponent") {
            EntityTypeComponent* comp = get<EntityTypeComponent>(entity);
            if (comp) return nb::cast(*comp);
        }
        if (component_name == "ItemTypeComponent") {
            ItemTypeComponent* comp = get<ItemTypeComponent>(entity);
            if (comp) return nb::cast(*comp);
        }

        if (component_name == "WeaponAttributes") {
            WeaponAttributes* comp = get<WeaponAttributes>(entity);
            if (comp) return nb::cast(*comp);
        }
        if (component_name == "Durability") {
            Durability* comp = get<Durability>(entity);
            if (comp) return nb::cast(*comp);
        }
        if (component_name == "MeeleAttackComponent") {
            MeeleAttackComponent* comp = get<MeeleAttackComponent>(entity);
            if (comp) return nb::cast(*comp);
        }
        if (component_name == "TileEffectComponent") {
            TileEffectComponent* comp = get<TileEffectComponent>(entity);
            if (comp) return nb::cast(*comp);
        }
        if (component_name == "TileEffectsList") {
            TileEffectsList* comp = get<TileEffectsList>(entity);
            if (comp) return nb::cast(*comp);
        }
        if (component_name == "ConsoleLogsComponent") {
            ConsoleLogsComponent* comp = get<ConsoleLogsComponent>(entity);
            if (comp) return nb::cast(*comp);
        }

        return nb::none();
    }

    // Set a component on an entity
    void set_component(uint32_t entity_id, const std::string& component_name,
                       nb::object component_obj) {
        entt::entity entity = entt::entity{entity_id};
        if (component_name == "Position") {
            Position comp = nb::cast<Position>(component_obj);
            set<Position>(entity, comp);
        }
        if (component_name == "EntityTypeComponent") {
            EntityTypeComponent comp = nb::cast<EntityTypeComponent>(component_obj);
            set<EntityTypeComponent>(entity, comp);
        }
        if (component_name == "Velocity") {
            Velocity comp = nb::cast<Velocity>(component_obj);
            set<Velocity>(entity, comp);
        }
        if (component_name == "MovingComponent") {
            MovingComponent comp = nb::cast<MovingComponent>(component_obj);
            set<MovingComponent>(entity, comp);
        }
        if (component_name == "Inventory") {
            Inventory comp = nb::cast<Inventory>(component_obj);
            set<Inventory>(entity, comp);
        }
        if (component_name == "MetabolismComponent") {
            MetabolismComponent comp = nb::cast<MetabolismComponent>(component_obj);
            set<MetabolismComponent>(entity, comp);
        }
        if (component_name == "DigestionComponent") {
            DigestionComponent comp = nb::cast<DigestionComponent>(component_obj);
            set<DigestionComponent>(entity, comp);
        }
        if (component_name == "HealthComponent") {
            HealthComponent comp = nb::cast<HealthComponent>(component_obj);
            set<HealthComponent>(entity, comp);
        }
        if (component_name == "ItemTypeComponent") {
            ItemTypeComponent comp = nb::cast<ItemTypeComponent>(component_obj);
            set<ItemTypeComponent>(entity, comp);
        }
        if (component_name == "WeaponAttributes") {
            WeaponAttributes comp = nb::cast<WeaponAttributes>(component_obj);
            set<WeaponAttributes>(entity, comp);
        }
        if (component_name == "Durability") {
            Durability comp = nb::cast<Durability>(component_obj);
            set<Durability>(entity, comp);
        }
        if (component_name == "MeeleAttackComponent") {
            MeeleAttackComponent comp = nb::cast<MeeleAttackComponent>(component_obj);
            set<MeeleAttackComponent>(entity, comp);
        }
        if (component_name == "TileEffectComponent") {
            TileEffectComponent comp = nb::cast<TileEffectComponent>(component_obj);
            set<TileEffectComponent>(entity, comp);
        }
        if (component_name == "TileEffectsList") {
            TileEffectsList comp = nb::cast<TileEffectsList>(component_obj);
            set<TileEffectsList>(entity, comp);
        }
        if (component_name == "ConsoleLogsComponent") {
            ConsoleLogsComponent comp = nb::cast<ConsoleLogsComponent>(component_obj);
            set<ConsoleLogsComponent>(entity, comp);
        }
    }

    void remove_component(uint32_t entity_id, const std::string& component_name) {
        entt::entity entity = entt::entity{entity_id};
        if (component_name == "Position") {
            remove<Position>(entity);
        }
        if (component_name == "Velocity") {
            remove<Velocity>(entity);
        }
        if (component_name == "MovingComponent") {
            remove<MovingComponent>(entity);
        }
        if (component_name == "Inventory") {
            remove<Inventory>(entity);
        }
        if (component_name == "MetabolismComponent") {
            remove<MetabolismComponent>(entity);
        }
        if (component_name == "DigestionComponent") {
            remove<DigestionComponent>(entity);
        }
        if (component_name == "HealthComponent") {
            remove<HealthComponent>(entity);
        }
        if (component_name == "ItemTypeComponent") {
            remove<ItemTypeComponent>(entity);
        }
        if (component_name == "WeaponAttributes") {
            remove<WeaponAttributes>(entity);
        }
        if (component_name == "Durability") {
            remove<Durability>(entity);
        }
        if (component_name == "MeeleAttackComponent") {
            remove<MeeleAttackComponent>(entity);
        }
        if (component_name == "TileEffectComponent") {
            remove<TileEffectComponent>(entity);
        }
        if (component_name == "TileEffectsList") {
            remove<TileEffectsList>(entity);
        }
        if (component_name == "ConsoleLogsComponent") {
            remove<ConsoleLogsComponent>(entity);
        }
    }

    bool is_valid(uint32_t entity_id) {
        entt::entity entity = entt::entity{entity_id};
        return registry.valid(entity);
    }

   private:
    entt::registry& registry;
    entt::dispatcher& dispatcher;

    // Helper to convert entities to a Python list
    template <typename View>
    nb::list entities_to_pylist(View view) {
        nb::list entity_list;
        for (auto entity : view) {
            entity_list.append(static_cast<uint32_t>(entity));
        }
        return entity_list;
    }
};

#endif  // PYREGISTRY_HPP