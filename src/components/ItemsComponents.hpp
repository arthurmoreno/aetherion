#ifndef ITEM_COMPONENTS_HPP
#define ITEM_COMPONENTS_HPP

#include <algorithm>
#include <entt/entt.hpp>
#include <iostream>
#include <map>
#include <stdexcept>
#include <tuple>
#include <vector>

enum struct ItemEnum { FOOD = 1, TOOL = 2, WEAPON = 3, ARMOR = 4, RESOURCE = 5 };

enum struct ItemFoodEnum { RASPBERRY_FRUIT = 1, RASPBERRY_LEAF = 2 };

enum struct ItemResourceEnum { RASPBERRY_BRANCH = 1 };

enum struct ItemToolEnum { STONE_AXE = 1 };

std::pair<int, int> splitStringToInts(const std::string& input);

struct ItemTypeComponent {
    int mainType;
    int subType0;
    int subType1;
};

struct FoodItem {
    float energyDensity;
    float mass;
    float volume;
    float energyHealthRatio;
    float convertionEfficiency;  // how much mass per turn is converted to energy
};

struct WeaponAttributes {
    int damage;
    int defense;
};

struct Durability {
    int current;
    int max;
};

struct Inventory {
    std::vector<int> itemIDs;  // Each element represents an itemID or -1 for empty
    int maxItems;              // Maximum number of items the inventory can hold

    int addItem(int itemID);
    bool addItemInSlot(int itemID, int slot);

    int popItem();
    bool removeItemById(int itemID);
    int removeItemBySlot(int slot);

    bool swapItems(int slot1, int slot2);

    bool isFull() const;
    bool isEmpty() const;

    int getItem(int slot) const;

    int currentItemCount() const;

    bool resizeInventory(int newSize);

    void clearInventory();
    void printInventory() const;
};

struct DropRates {
    std::map<std::string, std::tuple<float, int, int>> itemDropRates;

    void addItem(std::string itemID, float dropRate, int minDrop, int maxDrop);
};

#endif  // ITEM_COMPONENTS_HPP