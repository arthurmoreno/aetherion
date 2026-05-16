#ifndef ENTITYINTERFACE_HPP
#define ENTITYINTERFACE_HPP

#if !defined(__EMSCRIPTEN__)
#include <nanobind/nanobind.h>
#endif

#include <bitset>
#include <cstdint>
#include <cstring>
#include <entt/entt.hpp>
#include <tuple>
#include <type_traits>
#include <utility>
#include <ylt/struct_pack.hpp>

#include "components/ConsoleLogsComponent.hpp"
#include "components/DnaComponents.hpp"
#include "components/EntityTypeComponent.hpp"
#include "components/HealthComponents.hpp"
#include "components/ItemsComponents.hpp"
#include "components/MetabolismComponents.hpp"
#include "components/MovingComponent.hpp"
#include "components/PerceptionComponent.hpp"
#include "components/PhysicsComponents.hpp"
#include "components/PlantsComponents.hpp"
#include "components/TerrainComponents.hpp"

#if !defined(__EMSCRIPTEN__)
namespace nb = nanobind;
#endif

// Define readable component bitmask flags
enum ComponentFlag {
  ENTITY_TYPE = 0,
  MASS,
  POSITION,
  VELOCITY,
  MOVING_COMPONENT,
  HEALTH,
  PERCEPTION,
  INVENTORY,
  CONSOLE_LOGS_COMPONENT,
  MATTER_CONTAINER,
  ITEM_ENUM,
  FOOD_ITEM,
  PARENTS_COMPONENT,
  ITEMS_TYPE_COMPONENT,
  TILE_EFFECT_COMPONENT,
  TILE_EFFECTS_LIST,
  METABOLISM_COMPONENT,
  COMPONENT_COUNT // Automatically keeps count of the total components
};

// List of component types
using ComponentTypes =
    std::tuple<EntityTypeComponent, PhysicsStats, Position, Velocity,
               MovingComponent, HealthComponent, PerceptionComponent, Inventory,
               ConsoleLogsComponent, MatterContainer, ItemEnum, FoodItem,
               ParentsComponent, ItemTypeComponent, TileEffectComponent,
               TileEffectsList, MetabolismComponent>;

// Use fixed-width integer for cross-platform stable serialization (WASM vs.
// native)
struct EntityHeader {
  int entityId;
  std::uint64_t componentMask;
};

class EntityInterface {
public:
  int entityId = -1;
  std::bitset<COMPONENT_COUNT> componentMask; // Bitmask for component presence

  // Components stored in a tuple
  ComponentTypes components;

  // Mark components as present
  void addComponent(ComponentFlag flag) { componentMask.set(flag); }

  // Mark components as absent
  void removeComponent(ComponentFlag flag) { componentMask.reset(flag); }

  // Check if a component exists
  bool hasComponent(ComponentFlag flag) const {
    return componentMask.test(flag);
  }

  // Getters and setters for entityId
  int getEntityId() const { return entityId; }
  void setEntityId(int id) { entityId = id; }

  // Generic getter and setter for components
  template <typename Component> Component &getComponent() {
    return std::get<Component>(components);
  }

  template <typename Component> const Component &getComponent() const {
    return std::get<Component>(components);
  }

  template <typename Component> void setComponent(const Component &component) {
    std::get<Component>(components) = component;
    addComponent(componentFlag<Component>());
  }

  // Serialization function
  std::vector<char> serialize() const {
    std::vector<char> buffer;
    buffer.reserve(computeSerializedSize());
    // Serialize component mask as 64-bit to avoid width differences across
    // platforms
    EntityHeader header{entityId, componentMask.to_ullong()};
    struct_pack::serialize_to(buffer, header);

    serializeComponents(buffer);

    return buffer;
  }

  // Exact byte size of the serialized representation. Public so callers
  // that write into a pre-sized destination can ask for the size up front.
  size_t computeSerializedSize() const {
    EntityHeader header{entityId, componentMask.to_ullong()};
    size_t total = struct_pack::get_needed_size(header).size();
    addSerializedSizeForComponents(
        total,
        std::make_index_sequence<std::tuple_size<ComponentTypes>::value>{});
    return total;
  }

  // Writes the entity bytes into `dst[0..N-1]`, `N == computeSerializedSize()`.
  // Caller owns sizing the buffer; no growth, no bounds check. Produces the
  // same bytes as `serialize()`.
  size_t serializeInto(uint8_t *dst) const {
    size_t cursor = 0;
    writeHeaderInto(dst, cursor);
    writeComponentsInto(
        dst, cursor,
        std::make_index_sequence<std::tuple_size<ComponentTypes>::value>{});
    return cursor;
  }

  // Deserialization function
  static EntityInterface deserialize(const char *data, size_t size) {
    EntityInterface entityInterface;

    size_t offset = 0;
    size_t consume_len = 0;

    // Deserialize the header
    consume_len = 0;
    auto header_result = struct_pack::deserialize<EntityHeader>(
        data + offset, size - offset, consume_len);
    if (!header_result) {
      throw std::runtime_error("Failed to deserialize header");
    }
    offset += consume_len;

    EntityHeader header = header_result.value();
    entityInterface.entityId = header.entityId;
    entityInterface.componentMask =
        std::bitset<COMPONENT_COUNT>(header.componentMask);

    // Deserialize components
    entityInterface.deserializeComponents(data, size, offset);

    return entityInterface;
  }

  // Python serialization wrapper functions (disabled for Emscripten/WASM)
#if !defined(__EMSCRIPTEN__)
  nb::bytes py_serialize() const {
    std::vector<char> serialized_data = serialize();
    return nb::bytes(serialized_data.data(), serialized_data.size());
  }

  static EntityInterface py_deserialize(const nb::bytes &serialized_data) {
    const char *data_ptr = serialized_data.c_str();
    size_t data_size = serialized_data.size();

    if (data_size == 0) {
      throw std::runtime_error("Serialized data is empty");
    }

    return deserialize(data_ptr, data_size);
  }
#endif

private:
  // Helper to get the ComponentFlag for a given component type
  template <typename Component> static constexpr ComponentFlag componentFlag() {
    return componentFlagImpl<Component, ComponentTypes>(
        std::make_index_sequence<std::tuple_size<ComponentTypes>::value>{});
  }

  template <typename Component, typename Tuple, std::size_t... Is>
  static constexpr ComponentFlag componentFlagImpl(std::index_sequence<Is...>) {
    ComponentFlag flags[] = {static_cast<ComponentFlag>(Is)...};
    bool matches[] = {
        std::is_same<Component, std::tuple_element_t<Is, Tuple>>::value...};
    for (size_t i = 0; i < sizeof...(Is); ++i) {
      if (matches[i]) {
        return flags[i];
      }
    }
    // Should not reach here if Component is in ComponentTypes
    static_assert(sizeof...(Is) > 0,
                  "Component type not found in ComponentTypes");
    return COMPONENT_COUNT;
  }

  // Serialization of components
  void serializeComponents(std::vector<char> &buffer) const {
    serializeComponentsImpl(
        buffer,
        std::make_index_sequence<std::tuple_size<ComponentTypes>::value>{});
  }

  template <std::size_t... Is>
  void serializeComponentsImpl(std::vector<char> &buffer,
                               std::index_sequence<Is...>) const {
    (..., serializeComponent<std::tuple_element_t<Is, ComponentTypes>>(buffer));
  }

  // POD components: raw memcpy; non-POD: struct_pack. `deserializeComponent`
  // must stay symmetric on the same `is_trivially_copyable_v<C>` branch.
  template <typename Component>
  void serializeComponent(std::vector<char> &buffer) const {
    if (!hasComponent(componentFlag<Component>())) {
      return;
    }
    if constexpr (std::is_trivially_copyable_v<Component>) {
      const Component &c = getComponent<Component>();
      const size_t n = buffer.size();
      buffer.resize(n + sizeof(Component));
      std::memcpy(buffer.data() + n, &c, sizeof(Component));
    } else {
      struct_pack::serialize_to(buffer, getComponent<Component>());
    }
  }

  // Deserialization of components
  void deserializeComponents(const char *data, size_t size, size_t &offset) {
    deserializeComponentsImpl(
        data, size, offset,
        std::make_index_sequence<std::tuple_size<ComponentTypes>::value>{});
  }

  template <std::size_t... Is>
  void deserializeComponentsImpl(const char *data, size_t size, size_t &offset,
                                 std::index_sequence<Is...>) {
    (..., deserializeComponent<std::tuple_element_t<Is, ComponentTypes>>(
              data, size, offset));
  }

  template <typename Component>
  void deserializeComponent(const char *data, size_t size, size_t &offset) {
    if (!hasComponent(componentFlag<Component>())) {
      return;
    }
    if constexpr (std::is_trivially_copyable_v<Component>) {
      if (offset + sizeof(Component) > size) {
        throw std::runtime_error(
            "Buffer underflow during trivially-copyable component decode");
      }
      Component c;
      std::memcpy(&c, data + offset, sizeof(Component));
      offset += sizeof(Component);
      setComponent<Component>(c);
    } else {
      size_t consume_len = 0;
      auto result = struct_pack::deserialize<Component>(
          data + offset, size - offset, consume_len);
      if (!result) {
        throw std::runtime_error("Failed to deserialize component");
      }
      offset += consume_len;
      setComponent<Component>(result.value());
    }
  }

  template <std::size_t... Is>
  void addSerializedSizeForComponents(size_t &total,
                                      std::index_sequence<Is...>) const {
    (...,
     addSerializedSizeForComponent<std::tuple_element_t<Is, ComponentTypes>>(
         total));
  }

  template <typename Component>
  void addSerializedSizeForComponent(size_t &total) const {
    if (!hasComponent(componentFlag<Component>())) {
      return;
    }
    if constexpr (std::is_trivially_copyable_v<Component>) {
      total += sizeof(Component);
    } else {
      total += struct_pack::get_needed_size(getComponent<Component>()).size();
    }
  }

  // `struct_pack::writer_t`-satisfying adapter over a raw destination.
  struct RawByteWriter {
    char *ptr;
    void write(const char *data, std::size_t len) {
      std::memcpy(ptr, data, len);
      ptr += len;
    }
  };

  void writeHeaderInto(uint8_t *dst, size_t &cursor) const {
    EntityHeader header{entityId, componentMask.to_ullong()};
    const size_t hsz = struct_pack::get_needed_size(header).size();
    RawByteWriter w{reinterpret_cast<char *>(dst + cursor)};
    struct_pack::serialize_to(w, header);
    cursor += hsz;
  }

  template <std::size_t... Is>
  void writeComponentsInto(uint8_t *dst, size_t &cursor,
                           std::index_sequence<Is...>) const {
    (..., writeComponentInto<std::tuple_element_t<Is, ComponentTypes>>(dst,
                                                                       cursor));
  }

  template <typename Component>
  void writeComponentInto(uint8_t *dst, size_t &cursor) const {
    if (!hasComponent(componentFlag<Component>())) {
      return;
    }
    if constexpr (std::is_trivially_copyable_v<Component>) {
      const Component &c = getComponent<Component>();
      std::memcpy(dst + cursor, &c, sizeof(Component));
      cursor += sizeof(Component);
    } else {
      const Component &c = getComponent<Component>();
      const size_t sz = struct_pack::get_needed_size(c).size();
      RawByteWriter w{reinterpret_cast<char *>(dst + cursor)};
      struct_pack::serialize_to(w, c);
      cursor += sz;
    }
  }
};

// Pin the POD memcpy fast-path components. A future change that breaks
// trivial-copyability becomes a compile error here.
static_assert(std::is_trivially_copyable_v<EntityTypeComponent>);
static_assert(std::is_trivially_copyable_v<PhysicsStats>);
static_assert(std::is_trivially_copyable_v<Position>);
static_assert(std::is_trivially_copyable_v<Velocity>);
static_assert(std::is_trivially_copyable_v<MovingComponent>);
static_assert(std::is_trivially_copyable_v<HealthComponent>);
static_assert(std::is_trivially_copyable_v<PerceptionComponent>);
static_assert(std::is_trivially_copyable_v<MatterContainer>);
static_assert(std::is_trivially_copyable_v<ItemEnum>);
static_assert(std::is_trivially_copyable_v<FoodItem>);
static_assert(std::is_trivially_copyable_v<ItemTypeComponent>);
static_assert(std::is_trivially_copyable_v<TileEffectComponent>);
static_assert(std::is_trivially_copyable_v<MetabolismComponent>);

// Declare the template function
template <typename Component, typename Registry>
void try_add_component(Registry &registry, entt::entity entity,
                       EntityInterface &entityInterface, ComponentFlag flag);

// Declare the function to create an EntityInterface dynamically
EntityInterface createEntityInterface(entt::registry &registry,
                                      entt::entity entity);

#endif // ENTITYINTERFACE_HPP
