// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2019 The TurtleCoin developers
// Copyright (c) 2016-2020 The Karbo developers
// Copyright (c) 2018-2023 Conceal Network & Conceal Devs
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <functional>
#include <unordered_map>

#include <boost/functional/hash.hpp>
#include <list>
#include <boost/uuid/uuid.hpp>

#include <System/Context.h>
#include <System/ContextGroup.h>
#include <System/Dispatcher.h>
#include <System/Event.h>
#include <System/Timer.h>
#include <System/TcpConnection.h>
#include <System/TcpListener.h>

#include "CryptoNoteCore/OnceInInterval.h"
#include "CryptoNoteProtocol/CryptoNoteProtocolHandler.h"
#include "Common/CommandLine.h"
#include "Logging/LoggerRef.h"

#include "ConnectionContext.h"
#include "LevinProtocol.h"
#include "NetNodeCommon.h"
#include "NetNodeConfig.h"
#include "P2pProtocolDefinitions.h"
#include "P2pNetworks.h"
#include "PeerListManager.h"

namespace platform_system {
class TcpConnection;
}

namespace cn
{
  class LevinProtocol;
  class ISerializer;

  struct P2pMessage {
    enum Type {
      COMMAND,
      REPLY,
      NOTIFY
    };

    P2pMessage(Type type, uint32_t command, const BinaryArray& buffer, int32_t returnCode = 0) :
      type(type), command(command), buffer(buffer), returnCode(returnCode) {
    }

    size_t size() const {
      return buffer.size();
    }

    Type type;
    uint32_t command;
    const BinaryArray buffer;
    int32_t returnCode;
  };

  struct P2pConnectionContext : public CryptoNoteConnectionContext {
  public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    platform_system::Context<void>* context = nullptr;
    PeerIdType peerId = 0;
    platform_system::TcpConnection connection;

    P2pConnectionContext(platform_system::Dispatcher& dispatcher, logging::ILogger& log, platform_system::TcpConnection&& conn) :
      connection(std::move(conn)),
      logger(log, "node_server"),
      queueEvent(dispatcher) {
    }

    bool pushMessage(P2pMessage&& msg);
    std::vector<P2pMessage> popBuffer();
    void interrupt();

    uint64_t writeDuration(TimePoint now) const;

  private:
    logging::LoggerRef logger;
    TimePoint writeOperationStartTime;
    platform_system::Event queueEvent;
    std::vector<P2pMessage> writeQueue;
    size_t writeQueueSize = 0;
    bool stopped = false;
  };

  class NodeServer :  public IP2pEndpoint
  {
  public:

    static void init_options(boost::program_options::options_description& desc);

    NodeServer(platform_system::Dispatcher& dispatcher, cn::CryptoNoteProtocolHandler& payload_handler, logging::ILogger& log);

    bool run();
    bool init(const NetNodeConfig& config);
    bool deinit();
    bool sendStopSignal();
    uint32_t get_this_peer_port() const { return m_listeningPort; }
    cn::CryptoNoteProtocolHandler& get_payload_object();

    void serialize(ISerializer& s);

    // debug functions
    bool log_peerlist() const;
    bool log_connections() const;
    uint64_t get_connections_count() override;
    size_t get_outgoing_connections_count() const;

    cn::PeerlistManager& getPeerlistManager() { return m_peerlist; }

  private:
    enum PeerType
    {
      anchor = 0,
      white,
      gray
    };
    int handleCommand(const LevinProtocol::Command& cmd, BinaryArray& buff_out, P2pConnectionContext& context, bool& handled);

    //----------------- commands handlers ----------------------------------------------
    int handle_handshake(int command, const COMMAND_HANDSHAKE::request& arg, COMMAND_HANDSHAKE::response& rsp, P2pConnectionContext& context);
    int handle_timed_sync(int command, const COMMAND_TIMED_SYNC::request& arg, COMMAND_TIMED_SYNC::response& rsp, P2pConnectionContext& context);
    int handle_ping(int command, const COMMAND_PING::request& arg, COMMAND_PING::response& rsp, const P2pConnectionContext& context) const;

#ifdef ALLOW_DEBUG_COMMANDS
    int handle_get_stat_info(int command, COMMAND_REQUEST_STAT_INFO::request &arg, COMMAND_REQUEST_STAT_INFO::response &rsp, P2pConnectionContext &context);
    int handle_get_network_state(int command, COMMAND_REQUEST_NETWORK_STATE::request &arg, COMMAND_REQUEST_NETWORK_STATE::response &rsp, P2pConnectionContext &context);
    int handle_get_peer_id(int command, COMMAND_REQUEST_PEER_ID::request &arg, COMMAND_REQUEST_PEER_ID::response &rsp, P2pConnectionContext &context);
    bool check_trust(const proof_of_trust &tr);
#endif

    bool init_config();
    bool make_default_config();
    bool store_config();
    void initUpnp();

    bool handshake(cn::LevinProtocol& proto, P2pConnectionContext& context, bool just_take_peerlist = false);
    bool timedSync();
    bool handleTimedSyncResponse(const BinaryArray& in, P2pConnectionContext& context);
    void forEachConnection(const std::function<void(P2pConnectionContext &)> &action);

    void on_connection_new(P2pConnectionContext& context);
    void on_connection_close(P2pConnectionContext& context);

    //----------------- i_p2p_endpoint -------------------------------------------------------------
    void relay_notify_to_all(int command, const BinaryArray &data_buff, const net_connection_id *excludeConnection) override;
    bool invoke_notify_to_peer(int command, const BinaryArray &req_buff, const CryptoNoteConnectionContext &context) override;
    void drop_connection(CryptoNoteConnectionContext &context, bool add_fail) override;
    void for_each_connection(const std::function<void(cn::CryptoNoteConnectionContext &, PeerIdType)> &f) override;
    void externalRelayNotifyToAll(int command, const BinaryArray &data_buff, const net_connection_id *excludeConnection) override;
    void externalRelayNotifyToList(int command, const BinaryArray &data_buff, const std::list<boost::uuids::uuid> &relayList) override;
    //-----------------------------------------------------------------------------------------------
    bool handle_command_line(const boost::program_options::variables_map& vm);
    bool is_addr_recently_failed(const uint32_t address_ip);
    bool handleConfig(const NetNodeConfig& config);
    bool append_net_address(std::vector<NetworkAddress>& nodes, const std::string& addr);
    bool idle_worker();
    bool handle_remote_peerlist(const std::list<PeerlistEntry>& peerlist, time_t local_time, const CryptoNoteConnectionContext& context);
    bool get_local_node_data(basic_node_data& node_data) const;
    bool merge_peerlist_with_local(const std::list<PeerlistEntry>& bs);
    bool fix_time_delta(std::list<PeerlistEntry>& local_peerlist, time_t local_time, int64_t& delta) const;

    bool connections_maker();
    bool make_new_connection_from_peerlist(bool use_white_list);
    bool make_new_connection_from_anchor_peerlist(const std::vector<AnchorPeerlistEntry> &anchor_peerlist);
    bool try_to_connect_and_handshake_with_new_peer(const NetworkAddress &na, bool just_take_peerlist = false, uint64_t last_seen_stamp = 0, PeerType peer_type = white, uint64_t first_seen_stamp = 0);
    bool is_peer_used(const PeerlistEntry &peer) const;
    bool is_peer_used(const AnchorPeerlistEntry &peer) const;
    bool is_addr_connected(const NetworkAddress& peer) const;
    bool try_ping(const basic_node_data& node_data, const P2pConnectionContext& context);
    bool make_expected_connections_count(PeerType peer_type, size_t expected_connections);
    bool is_priority_node(const NetworkAddress& na);

    bool connect_to_peerlist(const std::vector<NetworkAddress>& peers);

    bool parse_peers_and_add_to_container(const boost::program_options::variables_map& vm,
      const command_line::arg_descriptor<std::vector<std::string> > & arg, std::vector<NetworkAddress>& container) const;

    //debug functions
    std::string print_connections_container() const;

    using ConnectionContainer = std::unordered_map<boost::uuids::uuid, P2pConnectionContext, boost::hash<boost::uuids::uuid>>;
    using ConnectionIterator = ConnectionContainer::iterator;
    ConnectionContainer m_connections;

    void acceptLoop();
    void connectionHandler(const boost::uuids::uuid& connectionId, P2pConnectionContext& connection);
    void writeHandler(P2pConnectionContext& ctx) const;
    void onIdle();
    void timedSyncLoop();
    void timeoutLoop();

    template<typename T>
    void safeInterrupt(T& obj) const;

    struct config
    {
      network_config m_net_config;
      uint64_t m_peer_id;

      void serialize(ISerializer& s) {
        KV_MEMBER(m_net_config)
        KV_MEMBER(m_peer_id)
      }
    };

    config m_config;
    std::string m_config_folder;

    bool m_have_address;
    bool m_first_connection_maker_call;
    uint32_t m_listeningPort;
    uint32_t m_external_port;
    uint32_t m_ip_address;
    bool m_allow_local_ip = false;
    bool m_hide_my_port = false;
    std::string m_p2p_state_filename;

    platform_system::Dispatcher& m_dispatcher;
    platform_system::ContextGroup m_workingContextGroup;
    platform_system::Event m_stopEvent;
    platform_system::Timer m_idleTimer;
    platform_system::Timer m_timeoutTimer;
    platform_system::TcpListener m_listener;
    logging::LoggerRef logger;
    std::atomic<bool> m_stop{false};

    CryptoNoteProtocolHandler& m_payload_handler;
    PeerlistManager m_peerlist;

    OnceInInterval m_connections_maker_interval = OnceInInterval(1);
    OnceInInterval m_peerlist_store_interval = OnceInInterval(60 * 30, false);
    platform_system::Timer m_timedSyncTimer;

    std::string m_bind_ip;
    std::string m_port;
#ifdef ALLOW_DEBUG_COMMANDS
    uint64_t m_last_stat_request_time;
#endif
    std::vector<NetworkAddress> m_priority_peers;
    std::vector<NetworkAddress> m_exclusive_peers;
    std::vector<NetworkAddress> m_seed_nodes;
    std::list<PeerlistEntry> m_command_line_peers;
    uint64_t m_peer_livetime;
    boost::uuids::uuid m_network_id = CRYPTONOTE_NETWORK;
    std::map<uint32_t, uint64_t> m_host_fails_score;
    mutable std::mutex mutex;
  };
}
