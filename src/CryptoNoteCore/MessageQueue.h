// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2021 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <queue>

#include "IntrusiveLinkedList.h"

#include "System/Event.h"
#include "System/InterruptedException.h"

namespace cn {

template<class MessageType> class MessageQueue {
public:
  MessageQueue(platform_system::Dispatcher& dispatcher);

  const MessageType& front();
  void pop();
  void push(const MessageType& message);

  void stop();

  typename IntrusiveLinkedList<MessageQueue<MessageType>>::hook& getHook();
  
private:
  void wait();
  std::queue<MessageType> messageQueue;
  platform_system::Event event;
  bool stopped;

  typename IntrusiveLinkedList<MessageQueue<MessageType>>::hook hook;
};

template<class MessageQueueContainer, class MessageType>
class MesageQueueGuard {
public:
  MesageQueueGuard(MessageQueueContainer& container, MessageQueue<MessageType>& messageQueue) : container(container), messageQueue(messageQueue) {
    container.addMessageQueue(messageQueue);
  }

  MesageQueueGuard(const MesageQueueGuard& other) = delete;
  MesageQueueGuard& operator=(const MesageQueueGuard& other) = delete;

  ~MesageQueueGuard() {
    container.removeMessageQueue(messageQueue);
  }
private:
  MessageQueueContainer& container;
  MessageQueue<MessageType>& messageQueue;
};

template<class MessageType>
MessageQueue<MessageType>::MessageQueue(platform_system::Dispatcher& dispatcher) : event(dispatcher), stopped(false) {}

template<class MessageType>
void MessageQueue<MessageType>::wait() {
  if (messageQueue.empty()) {
    if (stopped) {
      throw platform_system::InterruptedException();
    }

    event.clear();
    while (!event.get()) {
      event.wait();
    }
  }
}

template<class MessageType>
const MessageType& MessageQueue<MessageType>::front() {
  wait();
  return messageQueue.front();
}

template<class MessageType>
void MessageQueue<MessageType>::pop() {
  wait();
  messageQueue.pop();
}

template<class MessageType>
void MessageQueue<MessageType>::push(const MessageType& message) {
  messageQueue.push(message);
  event.set();
}

template<class MessageType>
void MessageQueue<MessageType>::stop() {
  stopped = true;
  event.set();
}

template<class MessageType>
typename IntrusiveLinkedList<MessageQueue<MessageType>>::hook& MessageQueue<MessageType>::getHook() {
  return hook;
}

}
