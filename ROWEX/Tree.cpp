#include <assert.h>
#include <algorithm>
#include <functional>
#include "Tree.h"
#include "N.cpp"
#include "../Epoche.cpp"

namespace ART_ROWEX {

    Tree::Tree(LoadKeyFunction loadKey) : root(new N256(0, {})), loadKey(loadKey) {
    }

    Tree::~Tree() {
        N::deleteChildren(root);
        N::deleteNode(root);
    }

    ThreadInfo Tree::getThreadInfo() {
        return ThreadInfo(this->epoche);
    }

    TID Tree::lookup(const Key &k, ThreadInfo &threadEpocheInfo) const {
        EpocheGuardReadonly epocheGuard(threadEpocheInfo);
        N *node = root;
        uint32_t level = 0;
        bool optimisticPrefixMatch = false;

        while (true) {
            switch (checkPrefix(node, k, level)) { // increases level
                case CheckPrefixResult::NoMatch:
                    return 0;
                case CheckPrefixResult::OptimisticMatch:
                    optimisticPrefixMatch = true;
                    // fallthrough
                case CheckPrefixResult::Match: {
                    if (k.getKeyLen() <= level) {
                        return 0;
                    }
                    node = N::getChild(k[level], node);

                    if (node == nullptr) {
                        return 0;
                    }
                    if (N::isLeaf(node)) {
                        TID tid = N::getLeaf(node);
                        if (level < k.getKeyLen() - 1 || optimisticPrefixMatch) {
                            return checkKey(tid, k);
                        } else {
                            return tid;
                        }
                    }
                }
            }
            level++;
        }
    }

    bool Tree::lookupRange(const Key &start, const Key &end, Key &continueKey, TID result[],
                                std::size_t resultSize, std::size_t &resultsFound, ThreadInfo &threadEpocheInfo) const {
        for (uint32_t i = 0; i < std::min(start.getKeyLen(), end.getKeyLen()); ++i) {
            if (start[i] > end[i]) {
                resultsFound = 0;
                return false;
            } else if (start[i] < end[i]) {
                break;
            }
        }
        EpocheGuard epocheGuard(threadEpocheInfo);
        TID toContinue = 0;
        bool restart;
        std::function<void(const N *)> copy = [&result, &resultSize, &resultsFound, &toContinue, &copy](const N *node) {
            if (N::isLeaf(node)) {
                if (resultsFound == resultSize) {
                    toContinue = N::getLeaf(node);
                    return;
                }
                result[resultsFound] = N::getLeaf(node);
                resultsFound++;
            } else {
                std::tuple<uint8_t, N *> children[256];
                uint32_t childrenCount = 0;
                N::getChildren(node, 0u, 255u, children, childrenCount);
                for (uint32_t i = 0; i < childrenCount; ++i) {
                    const N *n = std::get<1>(children[i]);
                    copy(n);
                    if (toContinue != 0) {
                        break;
                    }
                }
            }
        };
        std::function<void(const N *, uint32_t)> findStart = [&copy, &start, &findStart, &toContinue, &restart, this](
                const N *node, uint32_t level) {
            if (N::isLeaf(node)) {
                copy(node);
                return;
            }

            PCCompareResults prefixResult;
            prefixResult = checkPrefixCompare(node, start, level, loadKey);
            switch (prefixResult) {
                case PCCompareResults::Bigger:
                    copy(node);
                    break;
                case PCCompareResults::Equal: {
                    uint8_t startLevel = (start.getKeyLen() > level) ? start[level] : 0;
                    std::tuple<uint8_t, N *> children[256];
                    uint32_t childrenCount = 0;
                    N::getChildren(node, startLevel, 255, children, childrenCount);
                    for (uint32_t i = 0; i < childrenCount; ++i) {
                        const uint8_t k = std::get<0>(children[i]);
                        const N *n = std::get<1>(children[i]);
                        if (k == startLevel) {
                            findStart(n, level + 1);
                        } else if (k > startLevel) {
                            copy(n);
                        }
                        if (toContinue != 0 || restart) {
                            break;
                        }
                    }
                    break;
                }
                case PCCompareResults::SkippedLevel:
                    restart = true;
                    break;
                case PCCompareResults::Smaller:
                    break;
            }
        };
        std::function<void(const N *, uint32_t)> findEnd = [&copy, &end, &toContinue, &restart, &findEnd, this](
                const N *node, uint32_t level) {
            if (N::isLeaf(node)) {
                return;
            }
            PCCompareResults prefixResult;
            prefixResult = checkPrefixCompare(node, end, level, loadKey);

            switch (prefixResult) {
                case PCCompareResults::Smaller:
                    copy(node);
                    break;
                case PCCompareResults::Equal: {
                    uint8_t endLevel = (end.getKeyLen() > level) ? end[level] : 255;
                    std::tuple<uint8_t, N *> children[256];
                    uint32_t childrenCount = 0;
                    N::getChildren(node, 0, endLevel, children, childrenCount);
                    for (uint32_t i = 0; i < childrenCount; ++i) {
                        const uint8_t k = std::get<0>(children[i]);
                        const N *n = std::get<1>(children[i]);
                        if (k == endLevel) {
                            findEnd(n, level + 1);
                        } else if (k < endLevel) {
                            copy(n);
                        }
                        if (toContinue != 0 || restart) {
                            break;
                        }
                    }
                    break;
                }
                case PCCompareResults::Bigger:
                    break;
                case PCCompareResults::SkippedLevel:
                    restart = true;
                    break;
            }
        };

        restart:
        restart = false;
        resultsFound = 0;

        uint32_t level = 0;
        N *node = nullptr;
        N *nextNode = root;

        while (true) {
            node = nextNode;
            PCEqualsResults prefixResult;
            prefixResult = checkPrefixEquals(node, level, start, end, loadKey);
            switch (prefixResult) {
                case PCEqualsResults::SkippedLevel:
                    goto restart;
                case PCEqualsResults::NoMatch: {
                    return false;
                }
                case PCEqualsResults::Contained: {
                    copy(node);
                    break;
                }
                case PCEqualsResults::BothMatch: {
                    uint8_t startLevel = (start.getKeyLen() > level) ? start[level] : 0;
                    uint8_t endLevel = (end.getKeyLen() > level) ? end[level] : 255;
                    if (startLevel != endLevel) {
                        std::tuple<uint8_t, N *> children[256];
                        uint32_t childrenCount = 0;
                        N::getChildren(node, startLevel, endLevel, children, childrenCount);
                        for (uint32_t i = 0; i < childrenCount; ++i) {
                            const uint8_t k = std::get<0>(children[i]);
                            const N *n = std::get<1>(children[i]);
                            if (k == startLevel) {
                                findStart(n, level + 1);
                            } else if (k > startLevel && k < endLevel) {
                                copy(n);
                            } else if (k == endLevel) {
                                findEnd(n, level + 1);
                            }
                            if (restart) {
                                goto restart;
                            }
                            if (toContinue) {
                                break;
                            }
                        }
                    } else {
                        nextNode = N::getChild(startLevel, node);
                        level++;
                        continue;
                    }
                    break;
                }
            }
            break;
        }
        if (toContinue != 0) {
            loadKey(toContinue, continueKey);
            return true;
        } else {
            return false;
        }
    }


    TID Tree::checkKey(const TID tid, const Key &k) const {
        Key kt;
        this->loadKey(tid, kt);
        if (k == kt) {
            return tid;
        }
        return 0;
    }

    void Tree::insert(const Key &k, TID tid, ThreadInfo &epocheInfo) {
        EpocheGuard epocheGuard(epocheInfo);
        restart:
        bool needRestart = false;

        N *node = nullptr;
        N *nextNode = root;
        N *parentNode = nullptr;
        uint8_t parentKey, nodeKey = 0;
        uint32_t level = 0;

        while (true) {
            parentNode = node;
            parentKey = nodeKey;
            node = nextNode;
            auto v = node->getVersion();

            uint32_t nextLevel = level;

            uint8_t nonMatchingKey;
            Prefix remainingPrefix;
            switch (checkPrefixPessimistic(node, k, nextLevel, nonMatchingKey, remainingPrefix,
                                                           this->loadKey)) { // increases level
                case CheckPrefixPessimisticResult::SkippedLevel:
                    goto restart;
                case CheckPrefixPessimisticResult::NoMatch: {
                    assert(nextLevel < k.getKeyLen()); //prevent duplicate key
                    node->lockVersionOrRestart(v, needRestart);
                    if (needRestart) goto restart;

                    // 1) Create new node which will be parent of node, Set common prefix, level to this node
                    Prefix prefi = node->getPrefi();
                    prefi.prefixCount = nextLevel - level;
                    auto newNode = new N4(nextLevel, prefi);

                    // 2)  add node and (tid, *k) as children
                    newNode->insert(k[nextLevel], N::setLeaf(tid));
                    newNode->insert(nonMatchingKey, node);

                    // 3) lockVersionOrRestart, update parentNode to point to the new node, unlock
                    parentNode->writeLockOrRestart(needRestart);
                    if (needRestart) {
                        delete newNode;
                        node->writeUnlock();
                        goto restart;
                    }
                    N::change(parentNode, parentKey, newNode);
                    parentNode->writeUnlock();

                    // 4) update prefix of node, unlock
                    node->setPrefix(remainingPrefix.prefix,
                                    node->getPrefi().prefixCount - ((nextLevel - level) + 1));

                    node->writeUnlock();
                    return;
                }
                case CheckPrefixPessimisticResult::Match:
                    break;
            }
            assert(nextLevel < k.getKeyLen()); //prevent duplicate key
            level = nextLevel;
            nodeKey = k[level];
            nextNode = N::getChild(nodeKey, node);

            if (nextNode == nullptr) {
                node->lockVersionOrRestart(v, needRestart);
                if (needRestart) goto restart;

                N::insertAndUnlock(node, parentNode, parentKey, nodeKey, N::setLeaf(tid), epocheInfo, needRestart);
                if (needRestart) goto restart;
                return;
            }
            if (N::isLeaf(nextNode)) {
                node->lockVersionOrRestart(v, needRestart);
                if (needRestart) goto restart;

                Key key;
                loadKey(N::getLeaf(nextNode), key);

                level++;
                assert(level < key.getKeyLen()); //prevent inserting when prefix of key exists already
                uint32_t prefixLength = 0;
                while (key[level + prefixLength] == k[level + prefixLength]) {
                    prefixLength++;
                }

                auto n4 = new N4(level + prefixLength, &k[level], prefixLength);
                n4->insert(k[level + prefixLength], N::setLeaf(tid));
                n4->insert(key[level + prefixLength], nextNode);
                N::change(node, k[level - 1], n4);
                node->writeUnlock();
                return;
            }
            level++;
        }
    }

    void Tree::remove(const Key &k, TID tid, ThreadInfo &threadInfo) {
        EpocheGuard epocheGuard(threadInfo);
        restart:
        bool needRestart = false;

        N *node = nullptr;
        N *nextNode = root;
        N *parentNode = nullptr;
        uint8_t parentKey, nodeKey = 0;
        uint32_t level = 0;
        //bool optimisticPrefixMatch = false;

        while (true) {
            parentNode = node;
            parentKey = nodeKey;
            node = nextNode;
            auto v = node->getVersion();

            switch (checkPrefix(node, k, level)) { // increases level
                case CheckPrefixResult::NoMatch:
                    if (N::isObsolete(v) || !node->readUnlockOrRestart(v)) {
                        goto restart;
                    }
                    return;
                case CheckPrefixResult::OptimisticMatch:
                    // fallthrough
                case CheckPrefixResult::Match: {
                    nodeKey = k[level];
                    nextNode = N::getChild(nodeKey, node);

                    if (nextNode == nullptr) {
                        if (N::isObsolete(v) || !node->readUnlockOrRestart(v)) {//TODO benötigt??
                            goto restart;
                        }
                        return;
                    }
                    if (N::isLeaf(nextNode)) {
                        node->lockVersionOrRestart(v, needRestart);
                        if (needRestart) goto restart;

                        if (N::getLeaf(nextNode) != tid) {
                            node->writeUnlock();
                            return;
                        }
                        assert(parentNode == nullptr || node->getCount() != 1);
                        if (node->getCount() == 2 && node != root) {
                            // 1. check remaining entries
                            N *secondNodeN;
                            uint8_t secondNodeK;
                            std::tie(secondNodeN, secondNodeK) = N::getSecondChild(node, nodeKey);
                            if (N::isLeaf(secondNodeN)) {
                                parentNode->writeLockOrRestart(needRestart);
                                if (needRestart) {
                                    node->writeUnlock();
                                    goto restart;
                                }

                                //N::remove(node, k[level]); not necessary
                                N::change(parentNode, parentKey, secondNodeN);

                                parentNode->writeUnlock();
                                node->writeUnlockObsolete();
                                this->epoche.markNodeForDeletion(node, threadInfo);
                            } else {
                                uint64_t vChild = secondNodeN->getVersion();
                                secondNodeN->lockVersionOrRestart(vChild, needRestart);
                                if (needRestart) {
                                    node->writeUnlock();
                                    goto restart;
                                }
                                parentNode->writeLockOrRestart(needRestart);
                                if (needRestart) {
                                    node->writeUnlock();
                                    secondNodeN->writeUnlock();
                                    goto restart;
                                }

                                //N::remove(node, k[level]); not necessary
                                N::change(parentNode, parentKey, secondNodeN);
                                secondNodeN->addPrefixBefore(node, secondNodeK);

                                parentNode->writeUnlock();
                                node->writeUnlockObsolete();
                                this->epoche.markNodeForDeletion(node, threadInfo);
                                secondNodeN->writeUnlock();
                            }
                        } else {
                            N::removeAndUnlock(node, k[level], parentNode, parentKey, threadInfo, needRestart);
                            if (needRestart) goto restart;
                        }
                        return;
                    }
                    level++;
                }
            }
        }
    }


    typename Tree::CheckPrefixResult Tree::checkPrefix(N *n, const Key &k, uint32_t &level) {
        if (k.getKeyLen() <= n->getLevel()) {
            return CheckPrefixResult::NoMatch;
        }
        Prefix p = n->getPrefi();
        if (p.prefixCount + level < n->getLevel()) {
            level = n->getLevel();
            return CheckPrefixResult::OptimisticMatch;
        }
        if (p.prefixCount > 0) {
            for (uint32_t i = ((level + p.prefixCount) - n->getLevel());
                 i < std::min(p.prefixCount, maxStoredPrefixLength); ++i) {
                if (p.prefix[i] != k[level]) {
                    return CheckPrefixResult::NoMatch;
                }
                ++level;
            }
            if (p.prefixCount > maxStoredPrefixLength) {
                level += p.prefixCount - maxStoredPrefixLength;
                return CheckPrefixResult::OptimisticMatch;
            }
        }
        return CheckPrefixResult::Match;
    }

    typename Tree::CheckPrefixPessimisticResult Tree::checkPrefixPessimistic(N *n, const Key &k, uint32_t &level,
                                                                        uint8_t &nonMatchingKey,
                                                                        Prefix &nonMatchingPrefix,
                                                                        LoadKeyFunction loadKey) {
        Prefix p = n->getPrefi();
        if (p.prefixCount + level < n->getLevel()) {
            return CheckPrefixPessimisticResult::SkippedLevel;
        }
        if (p.prefixCount > 0) {
            uint32_t prevLevel = level;
            Key kt;
            for (uint32_t i = ((level + p.prefixCount) - n->getLevel()); i < p.prefixCount; ++i) {
                if (i == maxStoredPrefixLength) {
                    loadKey(N::getAnyChildTid(n), kt);
                }
                uint8_t curKey = i >= maxStoredPrefixLength ? kt[level] : p.prefix[i];
                if (curKey != k[level]) {
                    nonMatchingKey = curKey;
                    if (p.prefixCount > maxStoredPrefixLength) {
                        if (i < maxStoredPrefixLength) {
                            loadKey(N::getAnyChildTid(n), kt);
                        }
                        for (uint32_t j = 0; j < std::min((p.prefixCount - (level - prevLevel) - 1),
                                                          maxStoredPrefixLength); ++j) {
                            nonMatchingPrefix.prefix[j] = kt[level + j + 1];
                        }
                    } else {
                        for (uint32_t j = 0; j < p.prefixCount - i - 1; ++j) {
                            nonMatchingPrefix.prefix[j] = p.prefix[i + j + 1];
                        }
                    }
                    return CheckPrefixPessimisticResult::NoMatch;
                }
                ++level;
            }
        }
        return CheckPrefixPessimisticResult::Match;
    }

    typename Tree::PCCompareResults Tree::checkPrefixCompare(const N *n, const Key &k, uint32_t &level,
                                                        LoadKeyFunction loadKey) {
        Prefix p = n->getPrefi();
        if (p.prefixCount + level < n->getLevel()) {
            return PCCompareResults::SkippedLevel;
        }
        if (p.prefixCount > 0) {
            Key kt;
            for (uint32_t i = ((level + p.prefixCount) - n->getLevel()); i < p.prefixCount; ++i) {
                if (i == maxStoredPrefixLength) {
                    loadKey(N::getAnyChildTid(n), kt);
                }
                uint8_t kLevel = (k.getKeyLen() > level) ? k[level] : 0;

                uint8_t curKey = i >= maxStoredPrefixLength ? kt[level] : p.prefix[i];
                if (curKey < kLevel) {
                    return PCCompareResults::Smaller;
                } else if (curKey > kLevel) {
                    return PCCompareResults::Bigger;
                }
                ++level;
            }
        }
        return PCCompareResults::Equal;
    }

    typename Tree::PCEqualsResults Tree::checkPrefixEquals(const N *n, uint32_t &level, const Key &start, const Key &end,
                                                      LoadKeyFunction loadKey) {
        Prefix p = n->getPrefi();
        if (p.prefixCount + level < n->getLevel()) {
            return PCEqualsResults::SkippedLevel;
        }
        if (p.prefixCount > 0) {
            Key kt;
            for (uint32_t i = ((level + p.prefixCount) - n->getLevel()); i < p.prefixCount; ++i) {
                if (i == maxStoredPrefixLength) {
                    loadKey(N::getAnyChildTid(n), kt);
                }
                uint8_t startLevel = (start.getKeyLen() > level) ? start[level] : 0;
                uint8_t endLevel = (end.getKeyLen() > level) ? end[level] : 0;

                uint8_t curKey = i >= maxStoredPrefixLength ? kt[level] : p.prefix[i];
                if (curKey > startLevel && curKey < endLevel) {
                    return PCEqualsResults::Contained;
                } else if (curKey < startLevel || curKey > endLevel) {
                    return PCEqualsResults::NoMatch;
                }
                ++level;
            }
        }
        return PCEqualsResults::BothMatch;
    }
}