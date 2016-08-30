#include <assert.h>
#include <algorithm>
#include "N.h"
#include <emmintrin.h> // x86 SSE intrinsics

namespace ART_unsynchronized {
    const uint8_t N16_shrink = 3;

    bool N16::insert(uint8_t key, N *n) {
        if (count == 16) {
            return false;
        }
        uint8_t keyByteFlipped = flipSign(key);
        __m128i cmp = _mm_cmplt_epi8(_mm_set1_epi8(keyByteFlipped), _mm_loadu_si128(reinterpret_cast<__m128i *>(keys)));
        uint16_t bitfield = _mm_movemask_epi8(cmp) & (0xFFFF >> (16 - count));
        unsigned pos = bitfield ? ctz(bitfield) : count;
        memmove(keys + pos + 1, keys + pos, count - pos);
        memmove(children + pos + 1, children + pos, (count - pos) * sizeof(uintptr_t));
        keys[pos] = keyByteFlipped;
        children[pos] = n;
        count++;
        return true;
    }

    template<class NODE>
    void N16::copyTo(NODE *n) const {
        for (unsigned i = 0; i < count; i++) {
            n->insert(flipSign(keys[i]), children[i]);
        }
    }

    template<>
    void N16::copyTo(N4 *n) const {
        n->count = count;
        for (unsigned i = 0; i < N16_shrink; i++) {
            n->keys[i] = flipSign(keys[i]);
        }
        memcpy(n->children, children, sizeof(uintptr_t) * N16_shrink);
    }

    void N16::change(uint8_t key, N *val) {
        N **childPos = const_cast<N **>(getChildPos(key));
        assert(childPos != nullptr);
        *childPos = val;
    }

    N *const *N16::getChildPos(const uint8_t k) const {
        __m128i cmp = _mm_cmpeq_epi8(_mm_set1_epi8(flipSign(k)),
                                     _mm_loadu_si128(reinterpret_cast<const __m128i *>(keys)));
        unsigned bitfield = _mm_movemask_epi8(cmp) & ((1 << count) - 1);
        if (bitfield) {
            return &children[ctz(bitfield)];
        } else {
            return nullptr;
        }
    }

    N *N16::getChild(const uint8_t k) const {
        N *const *childPos = getChildPos(k);
        if (childPos == nullptr) {
            return nullptr;
        } else {
            return *childPos;
        }
    }

    bool N16::remove(uint8_t k, bool force) {
        if (count == N16_shrink && !force) {
            return false;
        }
        N *const *leafPlace = getChildPos(k);
        assert(leafPlace != nullptr);
        std::size_t pos = leafPlace - children;
        memmove(keys + pos, keys + pos + 1, count - pos - 1);
        memmove(children + pos, children + pos + 1, (count - pos - 1) * sizeof(N *));
        count--;
        assert(getChild(k) == nullptr);
        return true;
    }

    N *N16::getAnyChild() const {
        for (int i = 0; i < count; ++i) {
            if (N::isLeaf(children[i])) {
                return children[i];
            }
        }
        return children[0];
    }

    void N16::deleteChildren() {
        for (std::size_t i = 0; i < count; ++i) {
            N::deleteChildren(children[i]);
            N::deleteNode(children[i]);
        }
    }

    void N16::getChildren(uint8_t start, uint8_t end, std::tuple<uint8_t, N *> *&children,
                          uint32_t &childrenCount) const {
        childrenCount = 0;
        auto startPos = getChildPos(start);
        auto endPos = getChildPos(end);
        if (startPos == nullptr) {
            startPos = this->children;
        }
        if (endPos == nullptr) {
            endPos = this->children + (count - 1);
        }
        for (auto p = startPos; p <= endPos; ++p) {
            children[childrenCount] = std::make_tuple(flipSign(keys[p - this->children]), *p);
            childrenCount++;
        }
    }
}