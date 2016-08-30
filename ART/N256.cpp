#include <assert.h>
#include <algorithm>
#include "N.h"

namespace ART_unsynchronized {

    void N256::deleteChildren() {
        for (uint64_t i = 0; i < 256; ++i) {
            if (children[i] != nullptr) {
                N::deleteChildren(children[i]);
                N::deleteNode(children[i]);
            }
        }
    }

    bool N256::insert(uint8_t key, N *val) {
        children[key] = val;
        count++;
        return true;
    }

    template<class NODE>
    void N256::copyTo(NODE *n) const {
        for (int i = 0; i < 256; ++i) {
            if (children[i] != nullptr) {
                n->insert(i, children[i]);
            }
        }
    }

    void N256::change(uint8_t key, N *n) {
        children[key] = n;
    }

    N *N256::getChild(const uint8_t k) const {
        return children[k];
    }

    bool N256::remove(uint8_t k, bool force) {
        if (count == 37 && !force) {
            return false;
        }
        children[k] = nullptr;
        count--;
        return true;
    }

    N *N256::getAnyChild() const {
        N *anyChild = nullptr;
        for (uint64_t i = 0; i < 256; ++i) {
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

    void N256::getChildren(uint8_t start, uint8_t end, std::tuple<uint8_t, N *> *&children,
                           uint32_t &childrenCount) const {
        childrenCount = 0;
        for (unsigned i = start; i <= end; i++) {
            if (this->children[i] != nullptr) {
                children[childrenCount] = std::make_tuple(i, this->children[i]);
                childrenCount++;
            }
        }
    }
}