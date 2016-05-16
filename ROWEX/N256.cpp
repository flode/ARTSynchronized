#include <assert.h>
#include <algorithm>
#include "N.h"

namespace ART_ROWEX {

    void N256::deleteChildren() {
        for (uint64_t i = 0; i < 256; ++i) {
            if (children[i] != nullptr) {
                N::deleteChildren(children[i]);
                N::deleteNode(children[i]);
            }
        }
    }

    bool N256::insert(uint8_t key, N *val) {
        children[key].store(val, std::memory_order_release);
        count++;
        return true;
    }

    template<class NODE>
    void N256::copyTo(NODE *n) const {
        for (int i = 0; i < 256; ++i) {
            N *child = children[i].load();
            if (child != nullptr) {
                n->insert(i, child);
            }
        }
    }

    void N256::change(uint8_t key, N *n) {
        return children[key].store(n, std::memory_order_release);
    }

    N *N256::getChild(const uint8_t k) const {
        return children[k].load();
    }

    bool N256::remove(uint8_t k, bool force) {
        if (count == 37 && !force) {
            return false;
        }
        children[k].store(nullptr, std::memory_order_release);
        count--;
        return true;
    }

    N *N256::getAnyChild() const {
        N *anyChild = nullptr;
        for (uint64_t i = 0; i < 256; ++i) {
            N *child = children[i].load();
            if (child != nullptr) {
                if (N::isLeaf(child)) {
                    return child;
                } else {
                    anyChild = child;
                }
            }
        }
        return anyChild;
    }

    void N256::getChildren(uint8_t start, uint8_t end, std::tuple<uint8_t, N *> *&children,
                           uint32_t &childrenCount) const {
        childrenCount = 0;
        for (unsigned i = start; i <= end; i++) {
            N *child = this->children[i].load();
            if (child != nullptr) {
                children[childrenCount] = std::make_tuple(i, child);
                childrenCount++;
            }
        }
    }
}