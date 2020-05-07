// Copyright (c) 2018, The TurtleCoin Developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2020 Conceal Network & Conceal Devs
// 
// Please see the included LICENSE file for more information.

#pragma once

#include <P2p/P2pProtocolTypes.h>

#include <vector>

class Peerlist
{
    public:
        Peerlist(std::vector<PeerlistEntry> &peers, uint64_t maxSize);

        /* Gets the size of the peer list */
        uint64_t count() const;

        /* Gets a peer list entry, indexed by time */
        bool get(PeerlistEntry &entry, uint64_t index) const;

        /* Trim the peer list, removing the oldest ones */
        void trim();

    private:
        std::vector<PeerlistEntry>& m_peers;

        const uint64_t m_maxSize;
};