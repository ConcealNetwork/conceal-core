// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2022 Conceal Network & Conceal Devs
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>

#include <boost/program_options/variables_map.hpp>

#include "IWalletLegacy.h"
#include "PasswordContainer.h"

#include "Common/ConsoleHandler.h"
#include "CryptoNoteCore/CryptoNoteBasicImpl.h"
#include "CryptoNoteCore/Currency.h"
#include "NodeRpcProxy/NodeRpcProxy.h"
#include "WalletLegacy/WalletHelper.h"

#include <Logging/LoggerRef.h>
#include <Logging/LoggerManager.h>

#include <System/Dispatcher.h>
#include <System/Ipv4Address.h>

std::string remote_fee_address;
namespace cn
{
  /************************************************************************/
  /*                                                                      */
  /************************************************************************/
  class simple_wallet : public cn::INodeObserver, public cn::IWalletLegacyObserver, public cn::INodeRpcProxyObserver {
  public:
    simple_wallet(platform_system::Dispatcher& dispatcher, const cn::Currency& currency, logging::LoggerManager& log);

    bool init(const boost::program_options::variables_map& vm);
    bool deinit();
    bool run();
    void stop();

    bool process_command(const std::vector<std::string> &args);
    std::string get_commands_str(bool do_ext);
    std::string getFeeAddress();

    const cn::Currency& currency() const { return m_currency; }

  private:

    logging::LoggerMessage success_msg_writer(bool color = false) {
      return logger(logging::INFO, color ? logging::GREEN : logging::DEFAULT);
    }

    logging::LoggerMessage fail_msg_writer() const {
      auto msg = logger(logging::ERROR, logging::BRIGHT_RED);
      msg << "Error: ";
      return msg;
    }

    void handle_command_line(const boost::program_options::variables_map& vm);

    bool run_console_handler();

    bool new_wallet(const std::string &wallet_file, const std::string& password);
    bool new_wallet(crypto::SecretKey &secret_key, crypto::SecretKey &view_key, const std::string &wallet_file, const std::string& password);
    bool open_wallet(const std::string &wallet_file, const std::string& password);
    bool close_wallet();

    bool help(const std::vector<std::string> &args = std::vector<std::string>());
    bool extended_help(const std::vector<std::string> &args);
    bool exit(const std::vector<std::string> &args);
    bool show_dust(const std::vector<std::string> &args);
    bool show_balance(const std::vector<std::string> &args = std::vector<std::string>());
    bool sign_message(const std::vector<std::string> &args);
    bool verify_signature(const std::vector<std::string> &args);
    bool export_keys(const std::vector<std::string> &args = std::vector<std::string>());
    bool create_integrated(const std::vector<std::string> &args = std::vector<std::string>());
    bool show_incoming_transfers(const std::vector<std::string> &args);
    bool show_payments(const std::vector<std::string> &args);
    bool show_blockchain_height(const std::vector<std::string> &args);
    bool show_num_unlocked_outputs(const std::vector<std::string> &args);
    bool optimize_outputs(const std::vector<std::string> &args);
	  bool get_reserve_proof(const std::vector<std::string> &args);    
    bool get_tx_proof(const std::vector<std::string> &args);    
    bool optimize_all_outputs(const std::vector<std::string> &args);
    bool listTransfers(const std::vector<std::string> &args);
    bool transfer(const std::vector<std::string> &args);
    bool print_address(const std::vector<std::string> &args = std::vector<std::string>());
    bool save(const std::vector<std::string> &args);
    bool reset(const std::vector<std::string> &args);
    bool set_log(const std::vector<std::string> &args);
    bool save_keys_to_file(const std::vector<std::string> &args);

    bool deposit(const std::vector<std::string> &args);
    bool withdraw(const std::vector<std::string> &args);
    bool list_deposits(const std::vector<std::string> &args);

    std::string simple_menu();
    std::string extended_menu();

    std::string resolveAlias(const std::string& aliasUrl);
    void printConnectionError() const;

    std::string generate_mnemonic(crypto::SecretKey &);
    void log_incorrect_words(std::vector<std::string>);
    bool is_valid_mnemonic(std::string &, crypto::SecretKey &);


    //---------------- IWalletLegacyObserver -------------------------
    virtual void initCompleted(std::error_code result) override;
    virtual void externalTransactionCreated(cn::TransactionId transactionId) override;
    virtual void synchronizationCompleted(std::error_code result) override;
    virtual void synchronizationProgressUpdated(uint32_t current, uint32_t total) override;
    //----------------------------------------------------------

    //----------------- INodeRpcProxyObserver --------------------------
    virtual void connectionStatusUpdated(bool connected) override;
    //----------------------------------------------------------

    friend class refresh_progress_reporter_t;

    class refresh_progress_reporter_t
    {
    public:
      refresh_progress_reporter_t(cn::simple_wallet& simple_wallet)
        : m_simple_wallet(simple_wallet)
        , m_blockchain_height(0)
        , m_blockchain_height_update_time()
        , m_print_time()
      {
      }

      void update(uint64_t height, bool force = false)
      {
        auto current_time = std::chrono::system_clock::now();
        if (std::chrono::seconds(m_simple_wallet.currency().difficultyTarget() / 2) < current_time - m_blockchain_height_update_time ||
            m_blockchain_height <= height) {
          update_blockchain_height();
          m_blockchain_height = (std::max)(m_blockchain_height, height);
        }

        if (std::chrono::milliseconds(1) < current_time - m_print_time || force) {
          std::cout << "Height " << height << " of " << m_blockchain_height << '\r';
          m_print_time = current_time;
        }
      }

    private:
      void update_blockchain_height()
      {
        uint64_t blockchain_height = m_simple_wallet.m_node->getLastLocalBlockHeight();
        m_blockchain_height = blockchain_height;
        m_blockchain_height_update_time = std::chrono::system_clock::now();
      }

    private:
      cn::simple_wallet& m_simple_wallet;
      uint64_t m_blockchain_height;
      std::chrono::system_clock::time_point m_blockchain_height_update_time;
      std::chrono::system_clock::time_point m_print_time;
    };

  private:
    std::string m_wallet_file_arg;
    std::string m_generate_new;
    std::string m_import_new;
    std::string m_import_path;

    std::string m_daemon_address;
    std::string m_daemon_host;
    uint16_t m_daemon_port;

    std::string m_wallet_file;

    std::unique_ptr<std::promise<std::error_code>> m_initResultPromise;

    common::ConsoleHandler m_consoleHandler;
    const cn::Currency& m_currency;
    logging::LoggerManager& logManager;
    platform_system::Dispatcher& m_dispatcher;
    logging::LoggerRef logger;

    std::unique_ptr<cn::NodeRpcProxy> m_node;
    std::unique_ptr<cn::IWalletLegacy> m_wallet;
    refresh_progress_reporter_t m_refresh_progress_reporter;

    bool m_walletSynchronized;
    std::mutex m_walletSynchronizedMutex;
    std::condition_variable m_walletSynchronizedCV;
  };
}
