// Copyright (c) 2018, The TurtleCoin Developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2020 Conceal Network & Conceal Devs
// 
// Please see the included LICENSE file for more information.

#pragma once

#include <CryptoNoteConfig.h>

#include <list>

#include <P2p/P2pProtocolTypes.h>
#include <P2p/Peerlist.h>

#include <Serialization/ISerializer.h>

class PeerlistManager
{
    public:
        PeerlistManager();

        bool init(bool allow_local_ip);
        uint64_t get_white_peers_count() const { return m_peers_white.size(); }
        uint64_t get_gray_peers_count() const { return m_peers_gray.size(); }
        bool merge_peerlist(const std::list<PeerlistEntry>& outer_bs);
        bool get_peerlist_head(std::list<PeerlistEntry>& bs_head, uint32_t depth = CryptoNote::P2P_DEFAULT_PEERS_IN_HANDSHAKE);
        bool get_peerlist_full(std::list<PeerlistEntry>& pl_gray, std::list<PeerlistEntry>& pl_white) const;
        bool get_white_peer_by_index(PeerlistEntry& p, uint64_t i) const;
        bool get_gray_peer_by_index(PeerlistEntry& p, uint64_t i) const;
        bool append_with_peer_white(const PeerlistEntry& pr);
        bool append_with_peer_gray(const PeerlistEntry& pr);
        bool set_peer_just_seen(uint64_t peer, uint32_t ip, uint32_t port);
        bool set_peer_just_seen(uint64_t peer, const NetworkAddress& addr);
        bool set_peer_unreachable(const PeerlistEntry& pr);
        bool is_ip_allowed(uint32_t ip) const;
        void trim_white_peerlist();
        void trim_gray_peerlist();

        void serialize(CryptoNote::ISerializer& s);

        Peerlist& getWhite();
        Peerlist& getGray();

    private:
        std::string m_config_folder;
        bool m_allow_local_ip;
        std::vector<PeerlistEntry> m_peers_gray;
        std::vector<PeerlistEntry> m_peers_white;
        Peerlist m_whitePeerlist;
        Peerlist m_grayPeerlist;
};