#ifndef SCENEGRAPH_HPP
#define SCENEGRAPH_HPP

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <entt/entt.hpp>
#include <functional>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include "SceneComponents.hpp"

// -----------------------------------------------------------------------------
// SceneGraph:
//  - Uses entt::entity as the node handle (generation-safe).
//  - Stores a Hierarchy component on each participating entity.
//  - Maintains a vector of root entities for fast root iteration.
// -----------------------------------------------------------------------------
class SceneGraph {
   public:
    explicit SceneGraph(entt::registry* registry = nullptr) noexcept
        : owned_registry_{registry ? nullptr : std::make_unique<entt::registry>()},
          registry_{registry ? registry : owned_registry_.get()} {}

    // --- Node lifecycle -------------------------------------------------------

    // Create a new entity with an empty Hierarchy component. Starts as a root.
    entt::entity create_node() {
        auto e = registry_->create();
        registry_->emplace<Hierarchy>(e);  // default: all entt::null
        roots_.push_back(e);
        return e;
    }

    // Ensure an existing entity participates in the graph (if created elsewhere).
    // If it already has Hierarchy, this is a no-op. Otherwise, add it as a root.
    void ensure_node(entt::entity e) {
        if (!registry_->all_of<Hierarchy>(e)) {
            registry_->emplace<Hierarchy>(e);
            roots_.push_back(e);
        }
    }

    // Destroys an entire subtree rooted at 'e' (depth-first).
    // Removes it from the graph and destroys all entities in the subtree.
    void destroy_subtree(entt::entity e) {
        if (!registry_->valid(e) || !registry_->all_of<Hierarchy>(e)) {
            return;
        }
        // Detach from parent so roots_ stays consistent and siblings relink.
        detach(e);

        // Iterative DFS to collect nodes to destroy (children first).
        std::vector<entt::entity> stack;
        stack.push_back(e);

        while (!stack.empty()) {
            entt::entity cur = stack.back();
            stack.pop_back();

            // Push children
            auto& h = registry_->get<Hierarchy>(cur);
            for (entt::entity c = h.first_child; c != entt::null;) {
                auto& hc = registry_->get<Hierarchy>(c);
                entt::entity next = hc.next_sibling;
                stack.push_back(c);
                c = next;
            }
            // Remove hierarchy component and destroy entity
            registry_->remove<Hierarchy>(cur);
            // Also ensure 'cur' not listed as root
            erase_root_if_present_(cur);
            registry_->destroy(cur);
        }
    }

    // Destroys only 'e' and detaches it; children are adopted by its parent
    // in the same insertion position where 'e' lived (keeps order).
    void destroy_node_only(entt::entity e) {
        if (!registry_->valid(e) || !registry_->all_of<Hierarchy>(e)) {
            return;
        }
        // Save placement info
        auto& h = registry_->get<Hierarchy>(e);
        entt::entity p = h.parent;
        entt::entity before = h.next_sibling;  // reinsert children before where 'e' was

        // Move all children under p at 'before'
        while (h.first_child != entt::null) {
            entt::entity child = h.first_child;
            detach(child);  // Detach from e
            attach_child(p, child, before);
        }

        // Now detach & destroy e
        detach(e);
        registry_->remove<Hierarchy>(e);
        registry_->destroy(e);
    }

    // --- Attach / Detach / Reparent ------------------------------------------

    // Attach 'child' under 'parent'. If 'before' is entt::null, appends at end.
    // If 'parent' is entt::null, 'child' becomes a root.
    void attach_child(entt::entity parent, entt::entity child, entt::entity before = entt::null) {
        if (before != entt::null && static_cast<int>(before) == -1) {
            before = entt::null;
        }

        assert(registry_->valid(child));
        ensure_node(child);
        auto& hc = registry_->get<Hierarchy>(child);

        // Prevent cycles: can't attach an ancestor under its descendant
        assert(!is_descendant_of_(parent, child) && "Cannot create cycles in scene graph");

        // If child has a parent or is in roots, detach first.
        detach(child);

        // Root insertion
        if (parent == entt::null) {
            // Insert into roots_ before 'before' if before is also a root, else append.
            if (before != entt::null) {
                auto it = std::find(roots_.begin(), roots_.end(), before);
                if (it != roots_.end()) {
                    roots_.insert(it, child);
                } else {
                    roots_.push_back(child);
                }
            } else {
                roots_.push_back(child);
            }
            // parent, sibling fields are already null
            return;
        }

        // Ensure parent exists in graph
        ensure_node(parent);

        // Link as child of 'parent' keeping order via before
        auto& hp = registry_->get<Hierarchy>(parent);

        if (before == entt::null) {
            // append to end
            if (hp.first_child == entt::null) {
                hp.first_child = child;
                hc.parent = parent;
            } else {
                // find last sibling
                entt::entity last = hp.first_child;
                auto* hlast = &registry_->get<Hierarchy>(last);
                while (hlast->next_sibling != entt::null) {
                    last = hlast->next_sibling;
                    hlast = &registry_->get<Hierarchy>(last);
                }
                hlast->next_sibling = child;
                hc.prev_sibling = last;
                hc.parent = parent;
            }
        } else {
            // insert before 'before'
            assert(registry_->valid(before));
            auto& hb = registry_->get<Hierarchy>(before);
            assert(hb.parent == parent && "before must be a child of parent");

            entt::entity prev = hb.prev_sibling;
            hc.parent = parent;
            hc.next_sibling = before;
            hb.prev_sibling = child;

            if (prev == entt::null) {
                // inserting at head
                hp.first_child = child;
            } else {
                registry_->get<Hierarchy>(prev).next_sibling = child;
                hc.prev_sibling = prev;
            }
        }

        // If child was a root, remove it from roots_
        erase_root_if_present_(child);
    }

    void attach_node_python_instance(entt::entity node, nb::object instance) {
        if (!registry_->valid(node) || !registry_->all_of<Hierarchy>(node)) {
            throw std::runtime_error("Entity is not a valid node in the scene graph.");
        }
        if (registry_->all_of<NodePython>(node)) {
            // Update existing instance
            registry_->get<NodePython>(node).instance = instance;
        } else {
            // Add new NodePython component
            registry_->emplace<NodePython>(node, NodePython{instance});
        }
    }

    // Remove 'child' from its parent (or from roots). Does not destroy it.
    // After detach, 'child' becomes a root.
    void detach(entt::entity child) {
        if (!registry_->valid(child) || !registry_->all_of<Hierarchy>(child)) {
            return;
        }

        auto& hc = registry_->get<Hierarchy>(child);
        if (hc.parent == entt::null) {
            // It’s a root; ensure it’s in roots_ exactly once.
            add_root_if_absent_(child);
            // No sibling relinking needed when already a root with null siblings.
            return;
        }

        auto& hp = registry_->get<Hierarchy>(hc.parent);

        // Relink siblings
        if (hc.prev_sibling != entt::null) {
            registry_->get<Hierarchy>(hc.prev_sibling).next_sibling = hc.next_sibling;
        } else {
            // was first child
            hp.first_child = hc.next_sibling;
        }
        if (hc.next_sibling != entt::null) {
            registry_->get<Hierarchy>(hc.next_sibling).prev_sibling = hc.prev_sibling;
        }

        // Clear links and move to roots
        hc.parent = entt::null;
        hc.prev_sibling = entt::null;
        hc.next_sibling = entt::null;

        add_root_if_absent_(child);
    }

    // Move 'child' under 'new_parent' (or to roots if new_parent == null).
    // If 'before' is null, appends; otherwise inserts before that sibling.
    void reparent(entt::entity child, entt::entity new_parent, entt::entity before = entt::null) {
        if (!registry_->valid(child) || !registry_->all_of<Hierarchy>(child)) {
            return;
        }
        if (registry_->valid(new_parent) || new_parent == entt::null) {
            // NOP if already in desired spot?
            // We still detach/attach to maintain ordering as requested.
            detach(child);
            attach_child(new_parent, child, before);
        }
    }

    // --- Queries --------------------------------------------------------------

    static constexpr bool is_null(entt::entity e) noexcept { return e == entt::null; }

    bool contains(entt::entity e) const noexcept {
        return registry_->valid(e) && registry_->all_of<Hierarchy>(e);
    }

    entt::entity parent(entt::entity e) const noexcept {
        return contains(e) ? registry_->get<Hierarchy>(e).parent : entt::null;
    }

    entt::entity first_child(entt::entity e) const noexcept {
        return contains(e) ? registry_->get<Hierarchy>(e).first_child : entt::null;
    }

    entt::entity next_sibling(entt::entity e) const noexcept {
        return contains(e) ? registry_->get<Hierarchy>(e).next_sibling : entt::null;
    }

    entt::entity prev_sibling(entt::entity e) const noexcept {
        return contains(e) ? registry_->get<Hierarchy>(e).prev_sibling : entt::null;
    }

    bool is_root(entt::entity e) const noexcept {
        return contains(e) && registry_->get<Hierarchy>(e).parent == entt::null;
    }

    const std::vector<entt::entity>& roots() const noexcept { return roots_; }

    // Iterate direct children (stable order).
    template <typename Fn>
    void for_each_child(entt::entity e, Fn&& fn) const {
        if (!contains(e)) return;
        for (entt::entity c = first_child(e); c != entt::null; c = next_sibling(c)) {
            fn(c);
        }
    }

    // Preorder traversal of a subtree (includes root).
    template <typename Fn>
    void for_each_descendant_preorder(entt::entity root, Fn&& fn) const {
        if (!contains(root)) return;
        std::vector<entt::entity> stack;
        stack.push_back(root);
        while (!stack.empty()) {
            entt::entity cur = stack.back();
            stack.pop_back();
            fn(cur);
            // push children in reverse order so first_child is visited first
            std::vector<entt::entity> temp;
            for (entt::entity c = first_child(cur); c != entt::null; c = next_sibling(c)) {
                temp.push_back(c);
            }
            for (auto it = temp.rbegin(); it != temp.rend(); ++it) {
                stack.push_back(*it);
            }
        }
    }

    // Compute depth (root has depth 0). Returns -1 if not in graph.
    int depth(entt::entity e) const noexcept {
        if (!contains(e)) return -1;
        int d = 0;
        for (entt::entity p = parent(e); p != entt::null; p = parent(p)) {
            ++d;
        }
        return d;
    }

    // --- Rendering ------------------------------------------
    
    // Returns a list of node IDs in the correct order for rendering (preorder traversal)
    std::vector<entt::entity> get_render_order(entt::entity root) const {
        std::vector<entt::entity> render_list;
        
        if (!contains(root)) {
            return render_list;
        }
        
        for_each_descendant_preorder(root, [&render_list](entt::entity entity) {
            render_list.push_back(entity);
        });
        
        return render_list;
    }

    // --- Debugging ------------------------------------------------------------

    void drawGraphAsTree(entt::entity root) const {
        if (!contains(root)) {
            std::cout << "(empty graph)" << std::endl;
            return;
        }
        // Simple text-based tree drawing
        std::function<void(entt::entity, std::string)> draw_subtree;
        draw_subtree = [&](entt::entity node, std::string prefix) {
            std::cout << prefix << static_cast<int>(node) << std::endl;
            auto& h = registry_->get<Hierarchy>(node);
            for (entt::entity c = h.first_child; c != entt::null; c = next_sibling(c)) {
                draw_subtree(c, prefix + "  ");
            }
        };
        draw_subtree(root, "");
    }

   private:
    std::unique_ptr<entt::registry> owned_registry_{nullptr};
    entt::registry* registry_{nullptr};
    std::vector<entt::entity> roots_{};

    // --- Helpers --------------------------------------------------------------

    void add_root_if_absent_(entt::entity e) {
        auto it = std::find(roots_.begin(), roots_.end(), e);
        if (it == roots_.end()) roots_.push_back(e);
    }

    void erase_root_if_present_(entt::entity e) {
        auto it = std::find(roots_.begin(), roots_.end(), e);
        if (it != roots_.end()) roots_.erase(it);
    }

    // Returns true if 'candidate' is a descendant of 'ancestor'.
    bool is_descendant_of_(entt::entity candidate, entt::entity ancestor) const noexcept {
        if (candidate == entt::null || ancestor == entt::null) return false;
        for (entt::entity p = parent(candidate); p != entt::null; p = parent(p)) {
            if (p == ancestor) return true;
        }
        return false;
    }
};

#endif  // SCENEGRAPH_HPP