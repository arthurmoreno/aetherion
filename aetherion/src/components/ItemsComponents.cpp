#include "ItemsComponents.hpp"

std::pair<int, int> splitStringToInts(const std::string& input) {
    size_t colonPos = input.find(':');  // Find the position of the colon
    if (colonPos == std::string::npos) {
        throw std::invalid_argument("Input string must be in the format 'int:int'.");
    }

    // Extract the substrings before and after the colon
    std::string firstPart = input.substr(0, colonPos);
    std::string secondPart = input.substr(colonPos + 1);

    // Convert the substrings to integers
    int firstInt = std::stoi(firstPart);
    int secondInt = std::stoi(secondPart);

    return {firstInt, secondInt};  // Return as a pair of integers
}

// Adds an item to the first available empty slot
int Inventory::addItem(int itemID) {
    if (isFull()) {
        // Inventory is full
        return -1;
    }

    // Find the first available empty slot
    auto it = std::find(itemIDs.begin(), itemIDs.end(), -1);
    if (it != itemIDs.end()) {
        *it = itemID;
        return std::distance(itemIDs.begin(), it);
    }

    // This point should not be reached if isFull() works correctly
    return -1;
}

// Adds an item to a specific slot
bool Inventory::addItemInSlot(int itemID, int slot) {
    if (slot < 0 || slot >= maxItems) {
        // Invalid slot index
        return false;
    }
    if (itemIDs[slot] != -1) {
        // Slot is already occupied
        return false;
    }
    itemIDs[slot] = itemID;
    return true;
}

int Inventory::popItem() {
    int itemId = itemIDs.back();  // Retrieve the last item
    itemIDs.pop_back();

    return itemId;
}

// Removes an item by its itemID
bool Inventory::removeItemById(int itemID) {
    auto it = std::find(itemIDs.begin(), itemIDs.end(), itemID);
    if (it != itemIDs.end()) {
        *it = -1;
        return true;
    }
    return false;  // Item not found
}

// Removes an item from a specific slot
int Inventory::removeItemBySlot(int slot) {
    if (slot < 0 || slot >= maxItems) {
        // Invalid slot index
        return -1;
    }
    if (itemIDs[slot] == -1) {
        // Slot is already empty
        return -1;
    }
    int itemId = itemIDs[slot];
    itemIDs[slot] = -1;
    return itemId;
}

// Swaps items between two slots
bool Inventory::swapItems(int slot1, int slot2) {
    if (slot1 < 0 || slot1 >= maxItems || slot2 < 0 || slot2 >= maxItems) {
        // Invalid slot indices
        return false;
    }
    std::swap(itemIDs[slot1], itemIDs[slot2]);
    return true;
}

// Checks if the inventory is full
bool Inventory::isFull() const {
    // Inventory is full if no -1 is found
    return std::find(itemIDs.begin(), itemIDs.end(), -1) == itemIDs.end();
}

// Checks if the inventory is empty
bool Inventory::isEmpty() const {
    return std::all_of(itemIDs.begin(), itemIDs.end(), [](const int& id) { return id == -1; });
}

// Retrieves the itemID at a specific slot
int Inventory::getItem(int slot) const {
    if (slot < 0 || slot >= maxItems) {
        // Invalid slot index
        return -1;
    }
    return itemIDs[slot];
}

// Retrieves the current number of items in the inventory
int Inventory::currentItemCount() const {
    return std::count_if(itemIDs.begin(), itemIDs.end(), [](const int& id) { return id != -1; });
}

// Resizes the inventory to a new size
bool Inventory::resizeInventory(int newSize) {
    if (newSize < 0 || newSize > maxItems) {
        // Invalid new size
        return false;
    }
    if (newSize < static_cast<int>(itemIDs.size())) {
        // Truncate the inventory, setting truncated items to -1
        for (int i = newSize; i < itemIDs.size(); ++i) {
            itemIDs[i] = -1;
        }
    }
    itemIDs.resize(newSize, -1);
    return true;
}

// Clears all items from the inventory
void Inventory::clearInventory() { std::fill(itemIDs.begin(), itemIDs.end(), -1); }

// Prints the current state of the inventory
void Inventory::printInventory() const {
    for (int i = 0; i < maxItems; ++i) {
        if (itemIDs[i] != -1) {
            std::cout << "Slot " << i << ": Item ID " << itemIDs[i] << "\n";
        } else {
            std::cout << "Slot " << i << ": Empty\n";
        }
    }
}

void DropRates::addItem(std::string itemID, float dropRate, int minDrop, int maxDrop) {
    // Validate dropRate
    if (dropRate < 0.0f || dropRate > 1.0f) {
        throw std::invalid_argument("dropRate must be between 0 and 1.");
    }

    // Validate minDrop and maxDrop
    if (minDrop < 0 || maxDrop < 0) {
        throw std::invalid_argument("minDrop and maxDrop must be non-negative integers.");
    }
    if (minDrop > maxDrop) {
        throw std::invalid_argument("minDrop cannot be greater than maxDrop.");
    }

    // Add the item to the map
    itemDropRates[itemID] = std::make_tuple(dropRate, minDrop, maxDrop);
}