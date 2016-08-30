#include <assert.h>
#include <algorithm>
#include "N.h"

namespace ART_unsynchronized {

    void N4::deleteChildren() {
        for (uint32_t i = 0; i < 4; ++i) {
            if (children[i] != nullptr) {
                N::deleteChildren(children[i]);
                N::deleteNode(children[i]);
            }
        }
    }

    bool N4::insert(uint8_t key, N *n) {
        if (count == 4) {
            return false;
        }
        for (uint32_t i = 0; i < 4; ++i) {
            if (children[i] == nullptr) {
                keys[i] = key;
                children[i] = n;
                count++;
                return true;
            }
        }
        assert(false);
        __builtin_unreachable();
    }

    template<class NODE>
    void N4::copyTo(NODE *n) const {
        for (uint32_t i = 0; i < 4; ++i) {
            n->insert(keys[i], children[i]);
        }
    }

    void N4::change(uint8_t key, N *val) {
        for (uint32_t i = 0; i < 4; ++i) {
            if (children[i] != nullptr && keys[i] == key) {
                children[i] = val;
                return;
            }
        }
    }

    N *N4::getChild(const uint8_t k) const {
        for (uint32_t i = 0; i < 4; ++i) {
            if (children[i] != nullptr && keys[i] == k) {
                return children[i];
            }
        }
        return nullptr;
    }

    bool N4::remove(uint8_t k, bool /*force*/) {
        for (uint32_t i = 0; i < 4; ++i) {
            if (children[i] != nullptr && keys[i] == k) {
                count--;
                children[i] = nullptr;
                return true;
            }
        }
        assert(false);
        __builtin_unreachable();
    }

    N *N4::getAnyChild() const {
        N *anyChild;
        for (uint32_t i = 0; i < 4; ++i) {
            if (children[i] != nullptr) {
                if (N::isLeaf(children[i])) {
                    return children[i];
                } else {
                    anyChild = children[i];
                }
            }
        }
        return anyChild;
    }

    std::tuple<N *, uint8_t> N4::getSecondChild(const uint8_t key) const {
        for (uint32_t i = 0; i < 4; ++i) {
            if (children[i] != nullptr && keys[i] != key) {
                return std::make_tuple(children[i], keys[i]);
            }
        }
        assert(false);
        __builtin_unreachable();
    }

    void N4::getChildren(uint8_t start, uint8_t end, std::tuple<uint8_t, N *> *&children,
                         uint32_t &childrenCount) const {
//TODO lock
        childrenCount = 0;
        for (uint32_t i = 0; i < 4; ++i) {
            if (this->children[i] != nullptr && this->keys[i] >= start && this->keys[i] <= end) {
                children[childrenCount] = std::make_tuple(this->keys[i], this->children[i]);
                childrenCount++;
            }
        }
        std::sort(children, children + childrenCount, [](auto first, auto second) {
            return std::get<0>(first) < std::get<0>(second);
        });
    }
}