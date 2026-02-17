#include "EntityInterface.hpp"

// Helper function to fetch and add components dynamically
template <typename Component, typename Registry>
void try_add_component(Registry& registry, entt::entity entity, EntityInterface& entityInterface,
                       ComponentFlag flag) {
    if (auto component = registry.template try_get<Component>(entity)) {
        if constexpr (std::is_same_v<Component, Position>) {
            entityInterface.setComponent<Position>(*component);
        } else if constexpr (std::is_same_v<Component, Velocity>) {
            entityInterface.setComponent<Velocity>(*component);
        } else if constexpr (std::is_same_v<Component, PhysicsStats>) {
            entityInterface.setComponent<PhysicsStats>(*component);
        } else if constexpr (std::is_same_v<Component, HealthComponent>) {
            entityInterface.setComponent<HealthComponent>(*component);
        } else if constexpr (std::is_same_v<Component, PerceptionComponent>) {
            entityInterface.setComponent<PerceptionComponent>(*component);
        }
        entityInterface.addComponent(flag);
    }
}

// Function to create an EntityInterface dynamically by fetching components
EntityInterface createEntityInterface(entt::registry& registry, entt::entity entity) {
    EntityInterface entity_interface;
    entity_interface.entityId = static_cast<int>(entity);

    if (registry.all_of<EntityTypeComponent>(entity)) {
        const EntityTypeComponent& etc = registry.get<EntityTypeComponent>(entity);
        entity_interface.setComponent<EntityTypeComponent>(etc);
    }

    if (registry.all_of<Position>(entity)) {
        const Position& pos = registry.get<Position>(entity);
        entity_interface.setComponent<Position>(pos);
    }

    if (registry.all_of<Velocity>(entity)) {
        const Velocity& vel = registry.get<Velocity>(entity);
        entity_interface.setComponent<Velocity>(vel);
    }

    if (registry.all_of<PhysicsStats>(entity)) {
        const PhysicsStats& ps = registry.get<PhysicsStats>(entity);
        entity_interface.setComponent<PhysicsStats>(ps);
    }

    if (registry.all_of<HealthComponent>(entity)) {
        const HealthComponent& health = registry.get<HealthComponent>(entity);
        entity_interface.setComponent<HealthComponent>(health);
    }

    if (registry.all_of<PerceptionComponent>(entity)) {
        const PerceptionComponent& perception = registry.get<PerceptionComponent>(entity);
        entity_interface.setComponent<PerceptionComponent>(perception);
    }

    if (registry.all_of<Inventory>(entity)) {
        const Inventory& inventory = registry.get<Inventory>(entity);
        entity_interface.setComponent<Inventory>(inventory);
    }

    return entity_interface;
}