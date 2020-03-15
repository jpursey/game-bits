#include "gbits/message/message_system.h"

#include "gbits/message/message_dispatcher.h"
#include "gbits/message/message_endpoint.h"
#include "gbits/test/thread_tester.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

struct Counts {
  Counts() = default;
  int construct = 0;
  int destruct = 0;
  int add_message = 0;
  int counts[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
};

template <typename Dispatcher>
class TestDispatcher : public Dispatcher {
 public:
  TestDispatcher(Counts* counts) : counts_(counts) { counts_->construct += 1; }
  ~TestDispatcher() override { counts_->destruct += 1; }

  void AddMessage(MessageInternal internal,
                  const MessageDispatcher::Message& message) override {
    counts_->add_message += 1;
    Dispatcher::AddMessage(internal, message);
  }

 private:
  Counts* counts_;
};

class TestMessage {
 public:
  TestMessage(Counts* counts, int value = 0) : counts_(counts), value_(value) {
    counts_->construct += 1;
  }
  TestMessage(const TestMessage& other)
      : counts_(other.counts_), value_(other.value_) {
    counts_->construct += 1;
  }
  TestMessage(TestMessage&&) = delete;
  TestMessage& operator=(const TestMessage&) = delete;
  TestMessage& operator=(TestMessage&&) = delete;
  ~TestMessage() { counts_->destruct += 1; }

  int GetValue() const { return value_; }

 private:
  Counts* const counts_;
  const int value_;
};

TEST(MessageSystemTest, DefaultCreate) {
  auto message_system = MessageSystem::Create();
  ASSERT_NE(message_system, nullptr);
}

TEST(MessageSystemTest, CreateWithExternalDispatcher) {
  Counts counts;
  TestDispatcher<PollingMessageDispatcher> dispatcher(&counts);
  auto message_system = MessageSystem::Create(&dispatcher);
  ASSERT_NE(message_system, nullptr);
  EXPECT_EQ(dispatcher.GetSystem().Lock().Get(), message_system.get());
  message_system.reset();
  EXPECT_EQ(counts.destruct, 0);
  EXPECT_EQ(dispatcher.GetSystem().Lock().Get(), nullptr);
}

TEST(MessageSystemTest, CreateWithOwnedDispatcher) {
  Counts counts;
  auto dispatcher =
      std::make_unique<TestDispatcher<PollingMessageDispatcher>>(&counts);
  auto dispatcher_ptr = dispatcher.get();
  auto message_system = MessageSystem::Create(std::move(dispatcher));
  ASSERT_NE(message_system, nullptr);
  EXPECT_EQ(dispatcher_ptr->GetSystem().Lock().Get(), message_system.get());
  message_system.reset();
  EXPECT_EQ(counts.destruct, 1);
}

TEST(MessageSystemTest, CreateWithInvalidDispatcher) {
  PollingMessageDispatcher dispatcher;
  auto message_system_1 = MessageSystem::Create(&dispatcher);
  auto message_system_2 = MessageSystem::Create(&dispatcher);
  EXPECT_EQ(message_system_2, nullptr);
}

TEST(MessageSystemTest, CreateEndpoint) {
  auto message_system = MessageSystem::Create();
  auto endpoint = message_system->CreateEndpoint();
  ASSERT_NE(endpoint, nullptr);
  EXPECT_EQ(endpoint->GetSystem().Lock().Get(), message_system.get());
  EXPECT_NE(endpoint->GetId(), kNoMessageEndpointId);
  EXPECT_NE(endpoint->GetId(), kBroadcastMessageEndpointId);
  EXPECT_EQ(endpoint->GetName(), "");
  EXPECT_TRUE(message_system->IsValidEndpoint(endpoint->GetId()));
  EXPECT_EQ(message_system->GetEndpointType(endpoint->GetId()),
            MessageEndpointType::kEndpoint);
}

TEST(MessageSystemTest, CreateNamedEndpoint) {
  auto message_system = MessageSystem::Create();
  auto endpoint = message_system->CreateEndpoint("name");
  ASSERT_NE(endpoint, nullptr);
  EXPECT_EQ(endpoint->GetSystem().Lock().Get(), message_system.get());
  EXPECT_NE(endpoint->GetId(), kNoMessageEndpointId);
  EXPECT_NE(endpoint->GetId(), kBroadcastMessageEndpointId);
  EXPECT_EQ(endpoint->GetName(), "name");
  EXPECT_TRUE(message_system->IsValidEndpoint(endpoint->GetId()));
  EXPECT_EQ(message_system->GetEndpointType(endpoint->GetId()),
            MessageEndpointType::kEndpoint);
}

TEST(MessageSystemTest, CreateEndpointWithInvalidDispatcher) {
  PollingMessageDispatcher dispatcher;
  auto message_system_1 = MessageSystem::Create(&dispatcher);
  auto message_system_2 = MessageSystem::Create();
  auto endpoint = message_system_2->CreateEndpoint(&dispatcher);
  EXPECT_EQ(endpoint, nullptr);
}

TEST(MessageSystemTest, CreateEndpointWithSystemDispatcher) {
  PollingMessageDispatcher dispatcher;
  auto message_system = MessageSystem::Create(&dispatcher);
  auto endpoint = message_system->CreateEndpoint(&dispatcher);
  EXPECT_NE(endpoint, nullptr);
}

TEST(MessageSystemTest, CreateMultipleEndpointsWithSameDispatcher) {
  PollingMessageDispatcher dispatcher;
  auto message_system = MessageSystem::Create();
  auto endpoint_1 = message_system->CreateEndpoint(&dispatcher);
  auto endpoint_2 = message_system->CreateEndpoint(&dispatcher);
  EXPECT_NE(endpoint_1, nullptr);
  EXPECT_NE(endpoint_2, nullptr);
}

TEST(MessageSystemTest, DestroyEndpoint) {
  auto message_system = MessageSystem::Create();
  auto endpoint = message_system->CreateEndpoint();
  auto endpoint_id = endpoint->GetId();
  endpoint.reset();
  EXPECT_FALSE(message_system->IsValidEndpoint(endpoint_id));
  EXPECT_EQ(message_system->GetEndpointType(endpoint_id),
            MessageEndpointType::kInvalid);
}

TEST(MessageSystemTest, DestroySystemWithOrphanedEndpoints) {
  auto message_system = MessageSystem::Create();
  auto endpoint = message_system->CreateEndpoint();
  message_system.reset();
  EXPECT_EQ(endpoint->GetSystem().Lock().Get(), nullptr);
}

TEST(MessageSystemTest, SystemSendMessage) {
  int call_count = 0;
  MessageEndpointId result_from = kNoMessageEndpointId;
  int result_value = 0;
  auto message_system = MessageSystem::Create();
  auto endpoint = message_system->CreateEndpoint();
  endpoint->SetHandler<int>([&call_count, &result_from, &result_value](
                                MessageEndpointId from, const int& value) {
    call_count += 1;
    result_from = from;
    result_value = value;
  });
  EXPECT_TRUE(message_system->Send(endpoint->GetId(), 42));
  EXPECT_EQ(call_count, 1);
  EXPECT_EQ(result_from, kNoMessageEndpointId);
  EXPECT_EQ(result_value, 42);
}

TEST(MessageSystemTest, SystemSendMessagePollingDispatcher) {
  int call_count = 0;
  MessageEndpointId result_from = kNoMessageEndpointId;
  int result_value = 0;
  Counts counts;
  TestDispatcher<PollingMessageDispatcher> dispatcher(&counts);
  auto message_system = MessageSystem::Create(&dispatcher);
  auto endpoint = message_system->CreateEndpoint();
  endpoint->SetHandler<int>([&call_count, &result_from, &result_value](
                                MessageEndpointId from, const int& value) {
    call_count += 1;
    result_from = from;
    result_value = value;
  });
  EXPECT_TRUE(message_system->Send(endpoint->GetId(), 42));
  EXPECT_EQ(counts.add_message, 1);
  EXPECT_EQ(call_count, 0);
  dispatcher.Update();
  EXPECT_EQ(counts.add_message, 1);
  EXPECT_EQ(call_count, 1);
  EXPECT_EQ(result_from, kNoMessageEndpointId);
  EXPECT_EQ(result_value, 42);
}

TEST(MessageSystemTest, SystemSendMessageThreadDispatcher) {
  ThreadTester tester;
  int call_count = 0;
  MessageEndpointId result_from = kNoMessageEndpointId;
  int result_value = 0;
  Counts counts;
  TestDispatcher<ThreadMessageDispatcher> dispatcher(&counts);
  auto message_system = MessageSystem::Create(&dispatcher);
  auto endpoint = message_system->CreateEndpoint();
  endpoint->SetHandler<int>([&tester, &call_count, &result_from, &result_value](
                                MessageEndpointId from, const int& value) {
    tester.Wait(1);
    call_count += 1;
    result_from = from;
    result_value = value;
    tester.Signal(2);
  });
  EXPECT_TRUE(message_system->Send(endpoint->GetId(), 42));
  EXPECT_EQ(call_count, 0);
  tester.Signal(1);
  tester.Wait(2);
  EXPECT_EQ(counts.add_message, 1);
  EXPECT_EQ(call_count, 1);
  EXPECT_EQ(result_from, kNoMessageEndpointId);
  EXPECT_EQ(result_value, 42);
}

TEST(MessageSystemTest, EndpointSendMessage) {
  int call_count = 0;
  MessageEndpointId result_from = kNoMessageEndpointId;
  int result_value = 0;
  auto message_system = MessageSystem::Create();
  auto endpoint = message_system->CreateEndpoint();
  endpoint->SetHandler<int>([&call_count, &result_from, &result_value](
                                MessageEndpointId from, const int& value) {
    call_count += 1;
    result_from = from;
    result_value = value;
  });
  EXPECT_TRUE(endpoint->Send(endpoint->GetId(), 42));
  EXPECT_EQ(call_count, 1);
  EXPECT_EQ(result_from, endpoint->GetId());
  EXPECT_EQ(result_value, 42);
}

TEST(MessageSystemTest, EndpointSendMessagePollingDispatcher) {
  int call_count = 0;
  MessageEndpointId result_from = kNoMessageEndpointId;
  int result_value = 0;
  Counts counts;
  TestDispatcher<PollingMessageDispatcher> dispatcher(&counts);
  auto message_system = MessageSystem::Create();
  auto endpoint = message_system->CreateEndpoint(&dispatcher);
  endpoint->SetHandler<int>([&call_count, &result_from, &result_value](
                                MessageEndpointId from, const int& value) {
    call_count += 1;
    result_from = from;
    result_value = value;
  });
  EXPECT_TRUE(endpoint->Send(endpoint->GetId(), 42));
  EXPECT_EQ(counts.add_message, 1);
  EXPECT_EQ(call_count, 0);
  dispatcher.Update();
  EXPECT_EQ(counts.add_message, 1);
  EXPECT_EQ(call_count, 1);
  EXPECT_EQ(result_from, endpoint->GetId());
  EXPECT_EQ(result_value, 42);
}

TEST(MessageSystemTest, EndpointSendMessageThreadDispatcher) {
  ThreadTester tester;
  int call_count = 0;
  MessageEndpointId result_from = kNoMessageEndpointId;
  int result_value = 0;
  Counts counts;
  TestDispatcher<ThreadMessageDispatcher> dispatcher(&counts);
  auto message_system = MessageSystem::Create();
  auto endpoint = message_system->CreateEndpoint(&dispatcher);
  endpoint->SetHandler<int>([&tester, &call_count, &result_from, &result_value](
                                MessageEndpointId from, const int& value) {
    tester.Wait(1);
    call_count += 1;
    result_from = from;
    result_value = value;
    tester.Signal(2);
  });
  EXPECT_TRUE(endpoint->Send(endpoint->GetId(), 42));
  EXPECT_EQ(call_count, 0);
  tester.Signal(1);
  tester.Wait(2);
  EXPECT_EQ(counts.add_message, 1);
  EXPECT_EQ(call_count, 1);
  EXPECT_EQ(result_from, endpoint->GetId());
  EXPECT_EQ(result_value, 42);
}

TEST(MessageSystemTest, DeleteMessageInPollingDispatchAfterSystemDestruction) {
  Counts counts;
  TestMessage message(&counts);
  {
    PollingMessageDispatcher dispatcher;
    auto message_system = MessageSystem::Create(&dispatcher);
    auto endpoint = message_system->CreateEndpoint();
    endpoint->SetHandler<TestMessage>(
        [](MessageEndpointId, const TestMessage& message) {});
    message_system->Send(endpoint->GetId(), message);
    message_system.reset();
    EXPECT_EQ(counts.construct, 2);
    EXPECT_EQ(counts.destruct, 0);
  }
  EXPECT_EQ(counts.construct, 2);
  EXPECT_EQ(counts.destruct, 1);
}

TEST(MessageSystemTest, UpdateMessageInPollingDispatchAfterSystemDestruction) {
  Counts counts;
  TestMessage message(&counts);
  PollingMessageDispatcher dispatcher;
  auto message_system = MessageSystem::Create(&dispatcher);
  auto endpoint = message_system->CreateEndpoint();
  endpoint->SetHandler<TestMessage>(
      [](MessageEndpointId, const TestMessage& message) {});
  message_system->Send(endpoint->GetId(), message);
  message_system.reset();
  EXPECT_EQ(counts.construct, 2);
  EXPECT_EQ(counts.destruct, 0);
  dispatcher.Update();
  EXPECT_EQ(counts.construct, 2);
  EXPECT_EQ(counts.destruct, 1);
}

TEST(MessageSystemTest, DeleteMessageInThreadDispatchefterSystemDestruction) {
  Counts counts;
  TestMessage message(&counts);
  ThreadTester tester;
  {
    ThreadMessageDispatcher dispatcher;
    auto message_system = MessageSystem::Create(&dispatcher);
    auto endpoint = message_system->CreateEndpoint();
    endpoint->SetHandler<TestMessage>(
        [&tester, system_ptr = WeakPtr<MessageSystem>(message_system),
         endpoint_id = endpoint->GetId()](MessageEndpointId,
                                          const TestMessage& message) {
          auto system_lock = system_ptr.Lock();
          if (system_lock != nullptr) {
            // This ensures that a message will still be queued when the system
            // gets reset.
            system_lock->Send(endpoint_id, message);
          }
          tester.Signal(1);
          absl::SleepFor(absl::Milliseconds(10));
        });
    message_system->Send(endpoint->GetId(), message);
    tester.Wait(1);
    message_system.reset();
  }
  EXPECT_EQ(counts.construct, counts.destruct + 1);
  tester.Complete();
}

TEST(MessageSystemTest, DeleteMessageInThreadDispatchefterCancel) {
  Counts counts;
  TestMessage message(&counts);
  ThreadTester tester;
  ThreadMessageDispatcher dispatcher;
  auto message_system = MessageSystem::Create(&dispatcher);
  auto endpoint = message_system->CreateEndpoint();
  endpoint->SetHandler<TestMessage>(
      [&tester, &message_system, endpoint_id = endpoint->GetId()](
          MessageEndpointId, const TestMessage& message) {
        // This ensures that a message will still be queued when the system
        // gets reset.
        message_system->Send(endpoint_id, message);
        tester.Signal(1);
        absl::SleepFor(absl::Milliseconds(10));
      });
  message_system->Send(endpoint->GetId(), message);
  tester.Wait(1);
  dispatcher.Cancel();
  EXPECT_EQ(counts.construct, counts.destruct + 1);
  tester.Complete();
}

TEST(MessageSystemTest, SwitchDispatchersInSubscription) {
  Counts counts;
  TestMessage message(&counts);
  PollingMessageDispatcher endpoint_1_dispatcher;
  PollingMessageDispatcher endpoint_2_dispatcher;
  auto message_system = MessageSystem::Create();
  auto endpoint_1 = message_system->CreateEndpoint(&endpoint_1_dispatcher);
  endpoint_1->SetHandler<TestMessage>(
      [&counts](MessageEndpointId, const TestMessage& message) {
        counts.counts[0] += 1;
      });
  auto endpoint_2 = message_system->CreateEndpoint(&endpoint_2_dispatcher);
  endpoint_1->Subscribe(endpoint_2->GetId());
  message_system->Send(endpoint_2->GetId(), message);
  EXPECT_EQ(counts.construct, 2);
  EXPECT_EQ(counts.destruct, 0);
  EXPECT_EQ(counts.counts[0], 0);
  endpoint_2_dispatcher.Update();
  EXPECT_EQ(counts.construct, 3);
  EXPECT_EQ(counts.destruct, 1);
  EXPECT_EQ(counts.counts[0], 0);
  endpoint_2_dispatcher.Update();
  EXPECT_EQ(counts.construct, 3);
  EXPECT_EQ(counts.destruct, 1);
  EXPECT_EQ(counts.counts[0], 0);
  endpoint_1_dispatcher.Update();
  EXPECT_EQ(counts.construct, 3);
  EXPECT_EQ(counts.destruct, 2);
  EXPECT_EQ(counts.counts[0], 1);
}

TEST(MessageSystemTest, EndpointSendMessageNoSystem) {
  int call_count = 0;
  auto message_system = MessageSystem::Create();
  auto endpoint = message_system->CreateEndpoint();
  endpoint->SetHandler<int>(
      [&call_count](MessageEndpointId, const int&) { call_count += 1; });
  message_system.reset();
  EXPECT_FALSE(endpoint->Send(endpoint->GetId(), 42));
  EXPECT_EQ(call_count, 0);
}

TEST(MessageSystemTest, BroadcastMessage) {
  int call_count = 0;
  auto message_system = MessageSystem::Create();
  EXPECT_TRUE(message_system->IsValidEndpoint(kBroadcastMessageEndpointId));
  EXPECT_EQ(message_system->GetEndpointType(kBroadcastMessageEndpointId),
            MessageEndpointType::kChannel);
  auto handler = [&call_count](MessageEndpointId from, const int& message) {
    call_count += 1;
    EXPECT_EQ(from, kNoMessageEndpointId);
    EXPECT_EQ(message, 42);
  };
  auto endpoint_1 = message_system->CreateEndpoint();
  endpoint_1->SetHandler<int>(&handler);
  auto endpoint_2 = message_system->CreateEndpoint();
  endpoint_2->SetHandler<int>(&handler);
  EXPECT_TRUE(message_system->Send(kBroadcastMessageEndpointId, 42));
  EXPECT_EQ(call_count, 2);
}

TEST(MessageSystemTest, BroadcastChannelCannotBeRemoved) {
  auto message_system = MessageSystem::Create();
  EXPECT_FALSE(message_system->RemoveChannel(kBroadcastMessageEndpointId));
  EXPECT_EQ(message_system->GetEndpointType(kBroadcastMessageEndpointId),
            MessageEndpointType::kChannel);
}

TEST(MessageSystemTest, RemoveChannelTwice) {
  auto message_system = MessageSystem::Create();
  auto channel = message_system->AddChannel();
  EXPECT_TRUE(message_system->RemoveChannel(channel));
  EXPECT_FALSE(message_system->RemoveChannel(channel));
}

TEST(MessageSystemTest, RemoveNonChannel) {
  auto message_system = MessageSystem::Create();
  auto endpoint = message_system->CreateEndpoint();
  EXPECT_FALSE(message_system->RemoveChannel(endpoint->GetId()));
  EXPECT_EQ(message_system->GetEndpointType(endpoint->GetId()),
            MessageEndpointType::kEndpoint);
}

TEST(MessageSystemTest, SendChannelMessage) {
  int call_count = 0;
  auto message_system = MessageSystem::Create();
  auto handler = [&call_count](MessageEndpointId from, const int& message) {
    call_count += 1;
    EXPECT_EQ(from, kNoMessageEndpointId);
    EXPECT_EQ(message, 42);
  };
  auto channel = message_system->AddChannel();
  EXPECT_TRUE(message_system->IsValidEndpoint(channel));
  EXPECT_EQ(message_system->GetEndpointType(channel),
            MessageEndpointType::kChannel);
  auto endpoint_1 = message_system->CreateEndpoint();
  EXPECT_TRUE(endpoint_1->Subscribe(channel));
  EXPECT_TRUE(endpoint_1->IsSubscribed(channel));
  endpoint_1->SetHandler<int>(&handler);
  auto endpoint_2 = message_system->CreateEndpoint();
  EXPECT_TRUE(endpoint_2->Subscribe(channel));
  EXPECT_TRUE(endpoint_2->IsSubscribed(channel));
  endpoint_2->SetHandler<int>(&handler);
  // Endpoint 3 does *not* subscribe.
  auto endpoint_3 = message_system->CreateEndpoint();
  endpoint_3->SetHandler<int>(&handler);
  EXPECT_TRUE(message_system->Send(channel, 42));
  EXPECT_EQ(call_count, 2);
}

TEST(MessageSystemTest, SendToRemovedChannel) {
  int call_count = 0;
  auto message_system = MessageSystem::Create();
  auto handler = [&call_count](MessageEndpointId from, const int& message) {
    call_count += 1;
    EXPECT_EQ(from, kNoMessageEndpointId);
    EXPECT_EQ(message, 42);
  };
  auto channel = message_system->AddChannel();
  auto endpoint_1 = message_system->CreateEndpoint();
  EXPECT_TRUE(endpoint_1->Subscribe(channel));
  EXPECT_TRUE(endpoint_1->IsSubscribed(channel));
  endpoint_1->SetHandler<int>(&handler);
  auto endpoint_2 = message_system->CreateEndpoint();
  EXPECT_TRUE(endpoint_2->Subscribe(channel));
  EXPECT_TRUE(endpoint_2->IsSubscribed(channel));
  endpoint_2->SetHandler<int>(&handler);
  message_system->RemoveChannel(channel);
  EXPECT_FALSE(message_system->IsValidEndpoint(channel));
  EXPECT_EQ(message_system->GetEndpointType(channel),
            MessageEndpointType::kInvalid);
  EXPECT_FALSE(endpoint_1->IsSubscribed(channel));
  EXPECT_FALSE(endpoint_2->IsSubscribed(channel));
  EXPECT_FALSE(message_system->Send(channel, 42));
  EXPECT_EQ(call_count, 0);
}

TEST(MessageSystemTest, SendToUnsubscribedEndpoint) {
  int call_count = 0;
  auto message_system = MessageSystem::Create();
  auto handler = [&call_count](MessageEndpointId from, const int& message) {
    call_count += 1;
    EXPECT_EQ(from, kNoMessageEndpointId);
    EXPECT_EQ(message, 42);
  };
  auto channel = message_system->AddChannel();
  auto endpoint_1 = message_system->CreateEndpoint();
  EXPECT_TRUE(endpoint_1->Subscribe(channel));
  endpoint_1->SetHandler<int>(&handler);
  auto endpoint_2 = message_system->CreateEndpoint();
  EXPECT_TRUE(endpoint_2->Subscribe(channel));
  endpoint_2->SetHandler<int>(&handler);
  endpoint_1->Unsubscribe(channel);
  EXPECT_FALSE(endpoint_1->IsSubscribed(channel));
  EXPECT_TRUE(message_system->Send(channel, 42));
  EXPECT_EQ(call_count, 1);
}

TEST(MessageSystemTest, SendToDeletedEndpointViaChannel) {
  int call_count = 0;
  auto message_system = MessageSystem::Create();
  auto handler = [&call_count](MessageEndpointId from, const int& message) {
    call_count += 1;
    EXPECT_EQ(from, kNoMessageEndpointId);
    EXPECT_EQ(message, 42);
  };
  auto channel = message_system->AddChannel();
  auto endpoint_1 = message_system->CreateEndpoint();
  EXPECT_TRUE(endpoint_1->Subscribe(channel));
  endpoint_1->SetHandler<int>(&handler);
  auto endpoint_2 = message_system->CreateEndpoint();
  EXPECT_TRUE(endpoint_2->Subscribe(channel));
  endpoint_2->SetHandler<int>(&handler);
  endpoint_1.reset();
  EXPECT_TRUE(message_system->Send(channel, 42));
  EXPECT_EQ(call_count, 1);
}

TEST(MessageSystemTest, SendToDeletedEndpoint) {
  int call_count = 0;
  auto message_system = MessageSystem::Create();
  auto handler = [&call_count](MessageEndpointId from, const int& message) {
    call_count += 1;
    EXPECT_EQ(from, kNoMessageEndpointId);
    EXPECT_EQ(message, 42);
  };
  auto endpoint = message_system->CreateEndpoint();
  endpoint->SetHandler<int>(&handler);
  auto endpoint_id = endpoint->GetId();
  endpoint.reset();
  EXPECT_FALSE(message_system->Send(endpoint_id, 42));
  EXPECT_EQ(call_count, 0);
}

TEST(MessageSystemTest, SystemSendUncopiableMessage) {
  auto message_system = MessageSystem::Create();
  auto endpoint = message_system->CreateEndpoint();
  EXPECT_FALSE(message_system->Send<std::unique_ptr<int>>(
      endpoint->GetId(), std::make_unique<int>(42)));
}

TEST(MessageSystemTest, SubscribeToEndpoint) {
  int call_count = 0;
  auto message_system = MessageSystem::Create();
  auto handler = [&call_count](MessageEndpointId from, const int& message) {
    call_count += 1;
    EXPECT_EQ(from, kNoMessageEndpointId);
    EXPECT_EQ(message, 42);
  };
  auto endpoint_1 = message_system->CreateEndpoint();
  endpoint_1->SetHandler<int>(&handler);
  auto endpoint_2 = message_system->CreateEndpoint();
  EXPECT_TRUE(endpoint_2->Subscribe(endpoint_1->GetId()));
  EXPECT_TRUE(endpoint_2->IsSubscribed(endpoint_1->GetId()));
  EXPECT_FALSE(endpoint_1->IsSubscribed(endpoint_2->GetId()));
  endpoint_2->SetHandler<int>(&handler);
  EXPECT_TRUE(message_system->Send(endpoint_1->GetId(), 42));
  EXPECT_EQ(call_count, 2);
}

TEST(MessageSystemTest, SelfSubscription) {
  int call_count = 0;
  auto message_system = MessageSystem::Create();
  auto handler = [&call_count](MessageEndpointId from, const int& message) {
    call_count += 1;
    EXPECT_EQ(from, kNoMessageEndpointId);
    EXPECT_EQ(message, 42);
  };
  auto endpoint = message_system->CreateEndpoint();
  endpoint->SetHandler<int>(&handler);
  EXPECT_TRUE(endpoint->Subscribe(endpoint->GetId()));
  EXPECT_TRUE(endpoint->IsSubscribed(endpoint->GetId()));
  endpoint->SetHandler<int>(&handler);
  EXPECT_TRUE(message_system->Send(endpoint->GetId(), 42));
  EXPECT_EQ(call_count, 1);
}

TEST(MessageSystemTest, DuplicateSubscription) {
  int call_count = 0;
  auto message_system = MessageSystem::Create();
  auto handler = [&call_count](MessageEndpointId from, const int& message) {
    call_count += 1;
    EXPECT_EQ(from, kNoMessageEndpointId);
    EXPECT_EQ(message, 42);
  };
  auto endpoint_1 = message_system->CreateEndpoint();
  endpoint_1->SetHandler<int>(&handler);
  auto endpoint_2 = message_system->CreateEndpoint();
  EXPECT_TRUE(endpoint_2->Subscribe(endpoint_1->GetId()));
  EXPECT_TRUE(endpoint_2->Subscribe(endpoint_1->GetId()));
  EXPECT_TRUE(endpoint_2->IsSubscribed(endpoint_1->GetId()));
  EXPECT_FALSE(endpoint_1->IsSubscribed(endpoint_2->GetId()));
  endpoint_2->SetHandler<int>(&handler);
  EXPECT_TRUE(message_system->Send(endpoint_1->GetId(), 42));
  EXPECT_EQ(call_count, 2);
}

TEST(MessageSystemTest, RecursiveSubscription) {
  int call_count = 0;
  auto message_system = MessageSystem::Create();
  auto handler = [&call_count](MessageEndpointId from, const int& message) {
    call_count += 1;
    EXPECT_EQ(from, kNoMessageEndpointId);
    EXPECT_EQ(message, 42);
  };
  auto endpoint_1 = message_system->CreateEndpoint();
  endpoint_1->SetHandler<int>(&handler);
  auto endpoint_2 = message_system->CreateEndpoint();
  EXPECT_TRUE(endpoint_1->Subscribe(endpoint_2->GetId()));
  EXPECT_TRUE(endpoint_2->Subscribe(endpoint_1->GetId()));
  EXPECT_TRUE(endpoint_2->IsSubscribed(endpoint_1->GetId()));
  EXPECT_TRUE(endpoint_1->IsSubscribed(endpoint_2->GetId()));
  endpoint_2->SetHandler<int>(&handler);
  EXPECT_TRUE(message_system->Send(endpoint_1->GetId(), 42));
  EXPECT_EQ(call_count, 2);
  EXPECT_TRUE(message_system->Send(endpoint_2->GetId(), 42));
  EXPECT_EQ(call_count, 4);
}

TEST(MessageSystemTest, SubscribeToDeletedEndpoint) {
  int call_count = 0;
  auto message_system = MessageSystem::Create();
  auto handler = [&call_count](MessageEndpointId from, const int& message) {
    call_count += 1;
  };
  auto endpoint_1 = message_system->CreateEndpoint();
  endpoint_1->SetHandler<int>(&handler);
  auto endpoint_1_id = endpoint_1->GetId();
  endpoint_1.reset();
  auto endpoint_2 = message_system->CreateEndpoint();
  EXPECT_FALSE(endpoint_2->Subscribe(endpoint_1_id));
  EXPECT_FALSE(endpoint_2->IsSubscribed(endpoint_1_id));
  endpoint_2->Unsubscribe(endpoint_1_id);
  endpoint_2->SetHandler<int>(&handler);
  EXPECT_FALSE(message_system->Send(endpoint_1_id, 42));
  EXPECT_EQ(call_count, 0);
}

TEST(MessageSystemTest, SubscribeToEndpointThenDelete) {
  int call_count = 0;
  auto message_system = MessageSystem::Create();
  auto handler = [&call_count](MessageEndpointId from, const int& message) {
    call_count += 1;
  };
  auto endpoint_1 = message_system->CreateEndpoint();
  endpoint_1->SetHandler<int>(&handler);
  auto endpoint_1_id = endpoint_1->GetId();
  auto endpoint_2 = message_system->CreateEndpoint();
  EXPECT_TRUE(endpoint_2->Subscribe(endpoint_1_id));
  EXPECT_TRUE(endpoint_2->IsSubscribed(endpoint_1_id));
  endpoint_1.reset();
  EXPECT_FALSE(endpoint_2->IsSubscribed(endpoint_1_id));
  endpoint_2->SetHandler<int>(&handler);
  EXPECT_FALSE(message_system->Send(endpoint_1_id, 42));
  EXPECT_EQ(call_count, 0);
}

TEST(MessageSystemTest, SubscribeToRemovedChannel) {
  int call_count = 0;
  auto message_system = MessageSystem::Create();
  auto channel = message_system->AddChannel();
  message_system->RemoveChannel(channel);
  auto endpoint = message_system->CreateEndpoint();
  EXPECT_FALSE(endpoint->Subscribe(channel));
  endpoint->SetHandler<int>(
      [&call_count](MessageEndpointId from, const int& message) {
        call_count += 1;
      });
  EXPECT_FALSE(message_system->Send(channel, 42));
  EXPECT_EQ(call_count, 0);
}

TEST(MessageSystemTest, SendMessageWithoutDispatcherDoesNotCopy) {
  Counts counts;
  TestMessage message(&counts);
  auto message_system = MessageSystem::Create();
  auto endpoint = message_system->CreateEndpoint();
  endpoint->SetHandler<TestMessage>(
      [](MessageEndpointId, const TestMessage&) {});
  EXPECT_TRUE(message_system->Send(endpoint->GetId(), message));
  EXPECT_EQ(counts.construct, 1);
  EXPECT_EQ(counts.destruct, 0);
}

TEST(MessageSystemTest, SendMessageWithDispatcherCopies) {
  Counts counts;
  TestMessage message(&counts, 42);
  PollingMessageDispatcher dispatcher;
  auto message_system = MessageSystem::Create(&dispatcher);
  auto handler = [](MessageEndpointId, const TestMessage& message) {
    EXPECT_EQ(message.GetValue(), 42);
  };
  auto endpoint_1 = message_system->CreateEndpoint();
  endpoint_1->SetHandler<TestMessage>(handler);
  auto endpoint_2 = message_system->CreateEndpoint();
  endpoint_2->SetHandler<TestMessage>(handler);
  EXPECT_TRUE(message_system->Send(kBroadcastMessageEndpointId, message));
  EXPECT_EQ(counts.construct, 2);
  EXPECT_EQ(counts.destruct, 0);
  dispatcher.Update();
  EXPECT_EQ(counts.construct, 2);
  EXPECT_EQ(counts.destruct, 1);
}

TEST(MessageSystemTest, CreateEndpointInsideHandler) {
  Counts counts;
  auto message_system = MessageSystem::Create();
  auto endpoint = message_system->CreateEndpoint();
  std::unique_ptr<MessageEndpoint> new_endpoint;
  endpoint->SetHandler<int>(
      [&counts, &message_system, &endpoint, &new_endpoint](MessageEndpointId,
                                                           const int& message) {
        counts.counts[0] += message;
        if (new_endpoint != nullptr) {
          return;
        }
        new_endpoint = message_system->CreateEndpoint();
        EXPECT_TRUE(new_endpoint->Subscribe(endpoint->GetId()));
        new_endpoint->SetHandler<int>(
            [&counts](MessageEndpointId, const int& message) {
              counts.counts[1] += message;
            });
      });
  EXPECT_TRUE(message_system->Send(endpoint->GetId(), 1));
  EXPECT_EQ(counts.counts[0], 1);
  EXPECT_EQ(counts.counts[1], 0);
  EXPECT_TRUE(message_system->Send(endpoint->GetId(), 10));
  EXPECT_EQ(counts.counts[0], 11);
  EXPECT_EQ(counts.counts[1], 10);
}

TEST(MessageSystemTest, CreateEndpointInsideBroadcast) {
  Counts counts;
  auto message_system = MessageSystem::Create();
  auto endpoint = message_system->CreateEndpoint();
  std::unique_ptr<MessageEndpoint> new_endpoint;
  endpoint->SetHandler<int>(
      [&counts, &message_system, &endpoint, &new_endpoint](MessageEndpointId,
                                                           const int& message) {
        counts.counts[0] += message;
        if (new_endpoint != nullptr) {
          return;
        }
        new_endpoint = message_system->CreateEndpoint();
        EXPECT_TRUE(new_endpoint->Subscribe(endpoint->GetId()));
        new_endpoint->SetHandler<int>(
            [&counts](MessageEndpointId, const int& message) {
              counts.counts[1] += message;
            });
      });
  EXPECT_TRUE(message_system->Send(kBroadcastMessageEndpointId, 1));
  EXPECT_EQ(counts.counts[0], 1);
  EXPECT_EQ(counts.counts[1], 0);
  EXPECT_TRUE(message_system->Send(kBroadcastMessageEndpointId, 10));
  EXPECT_EQ(counts.counts[0], 11);
  EXPECT_EQ(counts.counts[1], 10);
}

TEST(MessageSystemTest, DestroyEndpointInsideHandler) {
  Counts counts;
  auto message_system = MessageSystem::Create();
  auto endpoint_1 = message_system->CreateEndpoint();
  auto endpoint_2 = message_system->CreateEndpoint();
  endpoint_1->SetHandler<int>(
      [&counts, &endpoint_2](MessageEndpointId, const int& message) {
        counts.counts[0] += message;
        endpoint_2.reset();
      });
  EXPECT_TRUE(endpoint_2->Subscribe(endpoint_1->GetId()));
  endpoint_2->SetHandler<int>([&counts](MessageEndpointId, const int& message) {
    counts.counts[1] += message;
  });
  EXPECT_TRUE(message_system->Send(endpoint_1->GetId(), 1));
  EXPECT_EQ(counts.counts[0], 1);
  EXPECT_EQ(counts.counts[1], 0);
  EXPECT_EQ(endpoint_2, nullptr);
}

TEST(MessageSystemTest, SubscribeUnsubscribeInsideHandler) {
  Counts counts;
  auto message_system = MessageSystem::Create();
  MessageEndpointId channel = message_system->AddChannel();
  auto endpoint_1 = message_system->CreateEndpoint();
  endpoint_1->SetHandler<int>([&counts, &message_system, &channel, &endpoint_1](
                                  MessageEndpointId, const int&) {
    counts.counts[0] += 1;
    EXPECT_TRUE(endpoint_1->Subscribe(channel));
    EXPECT_TRUE(endpoint_1->IsSubscribed(channel));
  });
  endpoint_1->SetHandler<float>(
      [&counts, &endpoint_1, &channel](MessageEndpointId, const float&) {
        counts.counts[1] += 1;
        endpoint_1->Unsubscribe(channel);
        EXPECT_FALSE(endpoint_1->IsSubscribed(channel));
        EXPECT_TRUE(endpoint_1->Subscribe(channel));
        EXPECT_TRUE(endpoint_1->IsSubscribed(channel));
        endpoint_1->Unsubscribe(channel);
        EXPECT_FALSE(endpoint_1->IsSubscribed(channel));
      });
  auto endpoint_2 = message_system->CreateEndpoint();
  endpoint_2->Subscribe(channel);
  endpoint_2->SetHandler<bool>(
      [&counts, &endpoint_1, &channel](MessageEndpointId, const float&) {
        counts.counts[2] += 1;
        EXPECT_TRUE(endpoint_1->Subscribe(channel));
        EXPECT_TRUE(endpoint_1->IsSubscribed(channel));
        endpoint_1->Unsubscribe(channel);
        EXPECT_FALSE(endpoint_1->IsSubscribed(channel));
        EXPECT_TRUE(endpoint_1->Subscribe(channel));
      });
  EXPECT_TRUE(message_system->Send(endpoint_1->GetId(), 1));
  EXPECT_TRUE(message_system->Send(channel, 1.0f));
  EXPECT_EQ(counts.counts[0], 1);
  EXPECT_EQ(counts.counts[1], 1);
  EXPECT_TRUE(message_system->Send(channel, 1.0f));
  EXPECT_EQ(counts.counts[0], 1);
  EXPECT_EQ(counts.counts[1], 1);
  EXPECT_TRUE(message_system->Send(channel, true));
  EXPECT_EQ(counts.counts[0], 1);
  EXPECT_EQ(counts.counts[1], 1);
  EXPECT_EQ(counts.counts[2], 1);
}

TEST(MessageSystemTest, AddRemoveChannelInsideHandler) {
  Counts counts;
  auto message_system = MessageSystem::Create();
  MessageEndpointId channel = kNoMessageEndpointId;
  auto endpoint = message_system->CreateEndpoint();
  endpoint->SetHandler<int>([&counts, &message_system, &channel, &endpoint](
                                MessageEndpointId, const int&) {
    counts.counts[0] += 1;
    channel = message_system->AddChannel();
    EXPECT_TRUE(endpoint->Subscribe(channel));
  });
  endpoint->SetHandler<float>(
      [&counts, &message_system, &channel](MessageEndpointId, const float&) {
        counts.counts[1] += 1;
        EXPECT_TRUE(message_system->RemoveChannel(channel));
      });
  EXPECT_TRUE(message_system->Send(endpoint->GetId(), 1));
  EXPECT_TRUE(message_system->Send(channel, 1.0f));
  EXPECT_EQ(counts.counts[0], 1);
  EXPECT_EQ(counts.counts[1], 1);
  EXPECT_FALSE(message_system->Send(channel, 1.0f));
  EXPECT_EQ(counts.counts[0], 1);
  EXPECT_EQ(counts.counts[1], 1);
}

TEST(MessageSystemTest, RemoveChannelTwiceInsideHandler) {
  Counts counts;
  auto message_system = MessageSystem::Create();
  MessageEndpointId channel = message_system->AddChannel();
  auto endpoint = message_system->CreateEndpoint();
  endpoint->Subscribe(channel);
  endpoint->SetHandler<int>(
      [&counts, &message_system, &channel](MessageEndpointId, const int&) {
        counts.counts[0] += 1;
        EXPECT_TRUE(message_system->RemoveChannel(channel));
        EXPECT_FALSE(message_system->RemoveChannel(channel));
      });
  EXPECT_TRUE(message_system->Send(channel, 1));
  EXPECT_EQ(counts.counts[0], 1);
}

TEST(MessageSystemTest, SendMessageInsideHandler) {
  Counts counts;
  auto message_system = MessageSystem::Create();
  auto endpoint = message_system->CreateEndpoint();
  endpoint->SetHandler<int>(
      [&counts, &message_system, &endpoint](MessageEndpointId, const int&) {
        counts.counts[0] += 1;
        EXPECT_TRUE(message_system->Send(endpoint->GetId(), 1.0f));
      });
  endpoint->SetHandler<float>(
      [&counts](MessageEndpointId, const float&) { counts.counts[1] += 1; });
  EXPECT_TRUE(message_system->Send(endpoint->GetId(), 1));
  EXPECT_EQ(counts.counts[0], 1);
  EXPECT_EQ(counts.counts[1], 1);
}

TEST(MessageSystemTest, CreateEndpointInsideHandlerWithPollingDispatcher) {
  Counts counts;
  PollingMessageDispatcher dispatcher;
  auto message_system = MessageSystem::Create(&dispatcher);
  auto endpoint = message_system->CreateEndpoint();
  std::unique_ptr<MessageEndpoint> new_endpoint;
  endpoint->SetHandler<int>(
      [&counts, &message_system, &endpoint, &new_endpoint](MessageEndpointId,
                                                           const int& message) {
        counts.counts[0] += message;
        if (new_endpoint != nullptr) {
          return;
        }
        new_endpoint = message_system->CreateEndpoint();
        EXPECT_TRUE(new_endpoint->Subscribe(endpoint->GetId()));
        new_endpoint->SetHandler<int>(
            [&counts](MessageEndpointId, const int& message) {
              counts.counts[1] += message;
            });
      });
  EXPECT_TRUE(message_system->Send(endpoint->GetId(), 1));
  dispatcher.Update();
  EXPECT_EQ(counts.counts[0], 1);
  EXPECT_EQ(counts.counts[1], 0);
  EXPECT_TRUE(message_system->Send(endpoint->GetId(), 10));
  dispatcher.Update();
  EXPECT_EQ(counts.counts[0], 11);
  EXPECT_EQ(counts.counts[1], 10);
}

TEST(MessageSystemTest, DestroyEndpointInsideHandlerWithPollingDispatcher) {
  Counts counts;
  PollingMessageDispatcher dispatcher;
  auto message_system = MessageSystem::Create(&dispatcher);
  auto endpoint_1 = message_system->CreateEndpoint();
  auto endpoint_2 = message_system->CreateEndpoint();
  endpoint_1->SetHandler<int>(
      [&counts, &endpoint_2](MessageEndpointId, const int& message) {
        counts.counts[0] += message;
        endpoint_2.reset();
      });
  EXPECT_TRUE(endpoint_2->Subscribe(endpoint_1->GetId()));
  endpoint_2->SetHandler<int>([&counts](MessageEndpointId, const int& message) {
    counts.counts[1] += message;
  });
  EXPECT_TRUE(message_system->Send(endpoint_1->GetId(), 1));
  dispatcher.Update();
  EXPECT_EQ(counts.counts[0], 1);
  EXPECT_EQ(counts.counts[1], 0);
  EXPECT_EQ(endpoint_2, nullptr);
}

TEST(MessageSystemTest, AddRemoveChannelInsideHandlerWithPollingDispatcher) {
  Counts counts;
  PollingMessageDispatcher dispatcher;
  auto message_system = MessageSystem::Create(&dispatcher);
  MessageEndpointId channel = kNoMessageEndpointId;
  auto endpoint = message_system->CreateEndpoint();
  endpoint->SetHandler<int>([&counts, &message_system, &channel, &endpoint](
                                MessageEndpointId, const int&) {
    counts.counts[0] += 1;
    channel = message_system->AddChannel();
    EXPECT_TRUE(endpoint->Subscribe(channel));
  });
  endpoint->SetHandler<float>(
      [&counts, &message_system, &channel](MessageEndpointId, const float&) {
        counts.counts[1] += 1;
        message_system->RemoveChannel(channel);
      });
  EXPECT_TRUE(message_system->Send(endpoint->GetId(), 1));
  dispatcher.Update();
  EXPECT_TRUE(message_system->Send(channel, 1.0f));
  dispatcher.Update();
  EXPECT_EQ(counts.counts[0], 1);
  EXPECT_EQ(counts.counts[1], 1);
  EXPECT_FALSE(message_system->Send(channel, 1.0f));
  dispatcher.Update();
  EXPECT_EQ(counts.counts[0], 1);
  EXPECT_EQ(counts.counts[1], 1);
}

TEST(MessageSystemTest, SendMessageInsideHandlerWithPollingDispatcher) {
  Counts counts;
  PollingMessageDispatcher dispatcher;
  auto message_system = MessageSystem::Create(&dispatcher);
  auto endpoint = message_system->CreateEndpoint();
  endpoint->SetHandler<int>(
      [&counts, &message_system, &endpoint](MessageEndpointId, const int&) {
        counts.counts[0] += 1;
        EXPECT_TRUE(message_system->Send(endpoint->GetId(), 1.0f));
      });
  endpoint->SetHandler<float>(
      [&counts](MessageEndpointId, const float&) { counts.counts[1] += 1; });
  EXPECT_TRUE(message_system->Send(endpoint->GetId(), 1));
  dispatcher.Update();
  EXPECT_EQ(counts.counts[0], 1);
  EXPECT_EQ(counts.counts[1], 1);
}

TEST(MessageSystemTest, CreateEndpointInsideHandlerWithThreadDispatcher) {
  Counts counts;
  ThreadTester tester;
  ThreadMessageDispatcher dispatcher;
  auto message_system = MessageSystem::Create(&dispatcher);
  auto endpoint = message_system->CreateEndpoint();
  std::unique_ptr<MessageEndpoint> new_endpoint;
  endpoint->SetHandler<int>([&counts, &message_system, &endpoint, &new_endpoint,
                             &tester](MessageEndpointId, const int& message) {
    counts.counts[0] += message;
    if (new_endpoint != nullptr) {
      tester.Signal(2);
      return;
    }
    new_endpoint = message_system->CreateEndpoint();
    EXPECT_TRUE(new_endpoint->Subscribe(endpoint->GetId()));
    new_endpoint->SetHandler<int>(
        [&counts, &tester](MessageEndpointId, const int& message) {
          counts.counts[1] += message;
          tester.Signal(3);
        });
    tester.Signal(1);
  });
  EXPECT_TRUE(message_system->Send(endpoint->GetId(), 1));
  tester.Wait(1);
  EXPECT_EQ(counts.counts[0], 1);
  EXPECT_EQ(counts.counts[1], 0);
  EXPECT_TRUE(message_system->Send(endpoint->GetId(), 10));
  tester.Wait(2);
  tester.Wait(3);
  EXPECT_EQ(counts.counts[0], 11);
  EXPECT_EQ(counts.counts[1], 10);
  tester.Complete();
}

TEST(MessageSystemTest, DestroyEndpointInsideHandlerWithThreadDispatcher) {
  Counts counts;
  ThreadTester tester;
  ThreadMessageDispatcher dispatcher;
  auto message_system = MessageSystem::Create(&dispatcher);
  auto endpoint_1 = message_system->CreateEndpoint();
  auto endpoint_2 = message_system->CreateEndpoint();
  endpoint_1->SetHandler<int>(
      [&counts, &endpoint_2, &tester](MessageEndpointId, const int& message) {
        counts.counts[0] += message;
        endpoint_2.reset();
        tester.Signal(1);
      });
  EXPECT_TRUE(endpoint_2->Subscribe(endpoint_1->GetId()));
  endpoint_2->SetHandler<int>([&counts](MessageEndpointId, const int& message) {
    counts.counts[1] += message;
  });
  EXPECT_TRUE(message_system->Send(endpoint_1->GetId(), 1));
  tester.Wait(1);
  absl::SleepFor(absl::Milliseconds(10));
  EXPECT_EQ(counts.counts[0], 1);
  EXPECT_EQ(counts.counts[1], 0);
  EXPECT_EQ(endpoint_2, nullptr);
  tester.Complete();
}

TEST(MessageSystemTest, AddRemoveChannelInsideHandlerWithThreadDispatcher) {
  Counts counts;
  ThreadTester tester;
  ThreadMessageDispatcher dispatcher;
  auto message_system = MessageSystem::Create(&dispatcher);
  MessageEndpointId channel = kNoMessageEndpointId;
  auto endpoint = message_system->CreateEndpoint();
  endpoint->SetHandler<int>([&counts, &message_system, &channel, &endpoint,
                             &tester](MessageEndpointId, const int&) {
    counts.counts[0] += 1;
    channel = message_system->AddChannel();
    EXPECT_TRUE(endpoint->Subscribe(channel));
    tester.Signal(1);
  });
  endpoint->SetHandler<float>([&counts, &message_system, &channel, &tester](
                                  MessageEndpointId, const float&) {
    counts.counts[1] += 1;
    message_system->RemoveChannel(channel);
    tester.Signal(2);
  });
  EXPECT_TRUE(message_system->Send(endpoint->GetId(), 1));
  tester.Wait(1);
  EXPECT_TRUE(message_system->Send(channel, 1.0f));
  tester.Wait(2);
  EXPECT_EQ(counts.counts[0], 1);
  EXPECT_EQ(counts.counts[1], 1);
  message_system->Send(channel, 1.0f);
  absl::SleepFor(absl::Milliseconds(10));
  EXPECT_EQ(counts.counts[0], 1);
  EXPECT_EQ(counts.counts[1], 1);
  tester.Complete();
}

TEST(MessageSystemTest, SendMessageInsideHandlerWithThreadDispatcher) {
  Counts counts;
  ThreadTester tester;
  ThreadMessageDispatcher dispatcher;
  auto message_system = MessageSystem::Create(&dispatcher);
  auto endpoint = message_system->CreateEndpoint();
  endpoint->SetHandler<int>([&counts, &message_system, &endpoint, &tester](
                                MessageEndpointId, const int&) {
    counts.counts[0] += 1;
    EXPECT_TRUE(message_system->Send(endpoint->GetId(), 1.0f));
    tester.Signal(1);
  });
  endpoint->SetHandler<float>(
      [&counts, &tester](MessageEndpointId, const float&) {
        counts.counts[1] += 1;
        tester.Signal(2);
      });
  EXPECT_TRUE(message_system->Send(endpoint->GetId(), 1));
  tester.Wait(1);
  tester.Wait(2);
  EXPECT_EQ(counts.counts[0], 1);
  EXPECT_EQ(counts.counts[1], 1);
  tester.Complete();
}

TEST(MessageSystemTest, RemoveEndpointFromOtherThread) {
  std::atomic<int> value = 0;
  ThreadTester tester;
  auto message_system = MessageSystem::Create();
  auto endpoint = message_system->CreateEndpoint();
  endpoint->SetHandler<int>([&value, &tester](MessageEndpointId, const int&) {
    tester.Signal(1);
    absl::SleepFor(absl::Milliseconds(10));
    value = 5;
  });
  tester.Run("handler", [&endpoint]() {
    EXPECT_TRUE(endpoint->Send(endpoint->GetId(), 1));
    return true;
  });
  tester.RunThenSignal(2, "remove", [&value, &tester, &endpoint]() {
    tester.Wait(1);
    endpoint.reset();
    EXPECT_EQ(value, 5);
    return true;
  });
  tester.Wait(2);
  tester.Complete();
}

TEST(MessageSystemTest, RemoveEndpointFromSubscribedEndpoint) {
  auto message_system = MessageSystem::Create();
  auto endpoint_1 = message_system->CreateEndpoint();
  auto endpoint_2 = message_system->CreateEndpoint();
  endpoint_2->Subscribe(endpoint_1->GetId());
  endpoint_2->SetHandler<int>(
      [&message_system, &endpoint_1](MessageEndpointId, const int&) {
        auto endpoint_id = endpoint_1->GetId();
        endpoint_1.reset();
        EXPECT_FALSE(message_system->IsValidEndpoint(endpoint_id));
        EXPECT_EQ(message_system->GetEndpointType(endpoint_id),
                  MessageEndpointType::kInvalid);
      });
  EXPECT_TRUE(message_system->Send(endpoint_1->GetId(), 1));
  EXPECT_EQ(endpoint_1, nullptr);
}

TEST(MessageSystemTest, RemoveEndpointOnThreadWhileDispatchingToSubscribers) {
  std::atomic<int> value = 0;
  ThreadTester tester;
  auto message_system = MessageSystem::Create();
  auto endpoint_1 = message_system->CreateEndpoint();
  auto endpoint_2 = message_system->CreateEndpoint();
  endpoint_2->Subscribe(endpoint_1->GetId());
  endpoint_2->SetHandler<float>(
      [&tester, &value](MessageEndpointId, const float&) {
        tester.Signal(1);
        tester.Wait(2);
        value = 5;
      });
  tester.Run("send", [&endpoint_1]() {
    // Send on endpoint_1, but receive in endpoint_2 so that endpoint_1 is still
    // being dispatched to, but it does not have a handler running currently.
    return endpoint_1->Send(endpoint_1->GetId(), 1.0f);
  });
  tester.RunThenSignal(3, "delete",
                       [&tester, &value, &message_system, &endpoint_1]() {
                         tester.Wait(1);
                         endpoint_1.reset();
                         EXPECT_EQ(value, 5);
                         return true;
                       });
  tester.Wait(1);
  absl::SleepFor(absl::Milliseconds(10));
  tester.Signal(2);
  tester.Wait(3);
  EXPECT_TRUE(tester.Complete()) << tester.GetResultString();
  EXPECT_EQ(endpoint_1, nullptr);
}

TEST(MessageSystemTest, EndpointSubscriptionOnDeletedSystem) {
  auto message_system = MessageSystem::Create();
  auto endpoint = message_system->CreateEndpoint();
  auto channel = message_system->AddChannel();
  EXPECT_TRUE(endpoint->Subscribe(channel));
  message_system.reset();
  EXPECT_FALSE(endpoint->IsSubscribed(channel));
  EXPECT_FALSE(endpoint->Subscribe(channel));
  endpoint->Unsubscribe(channel);
}

}  // namespace
}  // namespace gb
