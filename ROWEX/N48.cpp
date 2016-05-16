#include <assert.h>
#include <algorithm>
#include "N.h"

namespace ART_ROWEX {

    bool N48::insert(uint8_t key, N *n) {
        if (compactCount == 48) {
            return false;
        }
        children[compactCount].store(n, std::memory_order_release);
        childIndex[key].store(compactCount, std::memory_order_release);
        compactCount++;
        count++;
        return true;
    }

    template<class NODE>
    void N48::copyTo(NODE *n) const {
        for (unsigned i = 0; i < 256; i++) {
            uint8_t index = childIndex[i].load();
            if (index != emptyMarker) {
                n->insert(i, children[index]);
            }
        }
    }

    void N48::change(uint8_t key, N *val) {
        uint8_t index = childIndex[key].load();
        assert(index != emptyMarker);
        return children[index].store(val, std::memory_order_release);
    }

    N *N48::getChild(const uint8_t k) const {
        uint8_t index = childIndex[k].load();
        if (index == emptyMarker) {
            return nullptr;
        } else {
            return children[index].load();
        }
    }

    bool N48::remove(uint8_t k, bool force) {
        if (count == 12 && !force) {
            return false;
        }
        assert(childIndex[k] != emptyMarker);
        children[childIndex[k]].store(nullptr, std::memory_order_release);
        childIndex[k].store(emptyMarker, std::memory_order_release);
        count--;
        assert(getChild(k) == nullptr);
        return true;
    }

    N *N48::getAnyChild() const {
        N *anyChild = nullptr;
        for (unsigned i = 0; i < 48; i++) {
            N *child = children[i].load();
            if (child != nullptr) {
                if (N::isLeaf(child)) {
                    return child;
                }
                anyChild = child;
            }
        }
        return anyChild;
    }

    void N48::deleteChildren() {
        for (unsigned i = 0; i < 256; i++) {
            if (childIndex[i] != emptyMarker) {
                N::deleteChildren(children[childIndex[i]]);
                N::deleteNode(children[childIndex[i]]);
            }
        }
    }

    void N48::getChildren(uint8_t start, uint8_t end, std::tuple<uint8_t, N *> *&children,
                          uint32_t &childrenCount) const {
        childrenCount = 0;
        for (unsigned i = start; i <= end; i++) {
            uint8_t index = this->childIndex[i].load();
            if (index != emptyMarker) {
                N *child = this->children[index].load();
                if (child != nullptr) {
                    children[childrenCount] = std::make_tuple(i, child);
                    childrenCount++;
                }
            }
        }
    }
}