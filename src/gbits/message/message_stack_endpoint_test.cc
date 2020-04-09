#include "gbits/message/message_stack_endpoint.h"

#include "gbits/test/thread_tester.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

TEST(MessageStackTest, NullMessageSystemCreate) {
  EXPECT_EQ(MessageStackEndpoint::Create(nullptr, MessageStackOrder::kTopDown),
            nullptr);
  EXPECT_EQ(MessageStackEndpoint::Create(nullptr, MessageStackOrder::kTopDown,
                                         nullptr, {}),
            nullptr);
}

TEST(MessageStackTest, InvalidDispatcherCreate) {
  PollingMessageDispatcher dispatcher;
  auto message_system_1 = MessageSystem::Create(&dispatcher);
  auto message_system_2 = MessageSystem::Create();
  EXPECT_EQ(
      MessageStackEndpoint::Create(message_system_2.get(),
                                   MessageStackOrder::kTopDown, &dispatcher),
      nullptr);
}

TEST(MessageStackTest, EmptyMessageStack) {
  auto message_system = MessageSystem::Create();
  auto stack_endpoint = MessageStackEndpoint::Create(
      message_system.get(), MessageStackOrder::kTopDown, "name");
  ASSERT_NE(stack_endpoint, nullptr);
  EXPECT_NE(stack_endpoint->GetId(), kNoMessageEndpointId);
  EXPECT_NE(stack_endpoint->GetId(), kBroadcastMessageEndpointId);
  EXPECT_EQ(stack_endpoint->GetName(), "name");
  EXPECT_TRUE(stack_endpoint->IsSubscribed(kBroadcastMessageEndpointId));
  stack_endpoint->Unsubscribe(kBroadcastMessageEndpointId);
  EXPECT_FALSE(stack_endpoint->IsSubscribed(kBroadcastMessageEndpointId));
  EXPECT_TRUE(stack_endpoint->Subscribe(kBroadcastMessageEndpointId));
  EXPECT_TRUE(stack_endpoint->IsSubscribed(kBroadcastMessageEndpointId));
  EXPECT_TRUE(stack_endpoint->Send(stack_endpoint->GetId(), 42));
}

TEST(MessageStackTest, AddEmptyHandlersToStack) {
  MessageStackHandlers handlers;
  auto message_system = MessageSystem::Create();
  auto stack_endpoint = MessageStackEndpoint::Create(
      message_system.get(), MessageStackOrder::kTopDown);
  stack_endpoint->Push(&handlers);
  EXPECT_TRUE(stack_endpoint->Send(stack_endpoint->GetId(), 42));
}

TEST(MessageStackTest, InitHandlersThenAdd) {
  int count = 0;
  MessageStackHandlers handlers;
  handlers.SetHandler<int>(
      [&count](MessageEndpointId from, const int& message) {
        count += message;
        return true;
      });
  auto message_system = MessageSystem::Create();
  auto stack_endpoint = MessageStackEndpoint::Create(
      message_system.get(), MessageStackOrder::kTopDown);
  stack_endpoint->Push(&handlers);
  message_system->Send(stack_endpoint->GetId(), 42);
  EXPECT_EQ(count, 42);
}

TEST(MessageStackTest, AddHandlersThenInit) {
  int count = 0;
  MessageStackHandlers handlers;
  auto message_system = MessageSystem::Create();
  auto stack_endpoint = MessageStackEndpoint::Create(
      message_system.get(), MessageStackOrder::kTopDown);
  stack_endpoint->Push(&handlers);
  handlers.SetHandler<int>(
      [&count](MessageEndpointId from, const int& message) {
        count += message;
        return true;
      });
  message_system->Send(stack_endpoint->GetId(), 42);
  EXPECT_EQ(count, 42);
}

TEST(MessageStackTest, AddHandlersThenDelete) {
  int count = 0;
  auto handlers = std::make_unique<MessageStackHandlers>();
  handlers->SetHandler<int>(
      [&count](MessageEndpointId from, const int& message) {
        count += message;
        return true;
      });
  auto message_system = MessageSystem::Create();
  auto stack_endpoint = MessageStackEndpoint::Create(
      message_system.get(), MessageStackOrder::kTopDown);
  stack_endpoint->Push(handlers.get());
  message_system->Send(stack_endpoint->GetId(), 1);
  EXPECT_EQ(count, 1);
  handlers.reset();
  message_system->Send(stack_endpoint->GetId(), 1);
  EXPECT_EQ(count, 1);
}

TEST(MessageStackTest, AddHandlersThenRemove) {
  int count = 0;
  auto handlers = std::make_unique<MessageStackHandlers>();
  handlers->SetHandler<int>(
      [&count](MessageEndpointId from, const int& message) {
        count += message;
        return true;
      });
  auto message_system = MessageSystem::Create();
  auto stack_endpoint = MessageStackEndpoint::Create(
      message_system.get(), MessageStackOrder::kTopDown);
  stack_endpoint->Push(handlers.get());
  message_system->Send(stack_endpoint->GetId(), 1);
  EXPECT_EQ(count, 1);
  stack_endpoint->Remove(handlers.get());
  message_system->Send(stack_endpoint->GetId(), 1);
  EXPECT_EQ(count, 1);
}

TEST(MessageStackTest, HandlersProcessTopDown) {
  int count = 0;
  MessageStackHandlers handlers_1;
  handlers_1.SetHandler<int>(
      [&count](MessageEndpointId from, const int& message) {
        count += 1;
        return false;
      });
  MessageStackHandlers handlers_2;
  handlers_2.SetHandler<int>(
      [&count](MessageEndpointId from, const int& message) {
        count += 2;
        return true;
      });
  MessageStackHandlers handlers_3;
  handlers_3.SetHandler<int>(
      [&count](MessageEndpointId from, const int& message) {
        count += 4;
        return false;
      });
  auto message_system = MessageSystem::Create();
  auto stack_endpoint = MessageStackEndpoint::Create(
      message_system.get(), MessageStackOrder::kTopDown);
  stack_endpoint->Push(&handlers_3);
  stack_endpoint->Push(&handlers_2);
  stack_endpoint->Push(&handlers_1);
  message_system->Send(stack_endpoint->GetId(), 1);
  EXPECT_EQ(count, 3);
}

TEST(MessageStackTest, HandlersProcessBottomUp) {
  int count = 0;
  MessageStackHandlers handlers_1;
  handlers_1.SetHandler<int>(
      [&count](MessageEndpointId from, const int& message) {
        count += 1;
        return false;
      });
  MessageStackHandlers handlers_2;
  handlers_2.SetHandler<int>(
      [&count](MessageEndpointId from, const int& message) {
        count += 2;
        return true;
      });
  MessageStackHandlers handlers_3;
  handlers_3.SetHandler<int>(
      [&count](MessageEndpointId from, const int& message) {
        count += 4;
        return false;
      });
  auto message_system = MessageSystem::Create();
  auto stack_endpoint = MessageStackEndpoint::Create(
      message_system.get(), MessageStackOrder::kBottomUp);
  stack_endpoint->Push(&handlers_3);
  stack_endpoint->Push(&handlers_2);
  stack_endpoint->Push(&handlers_1);
  message_system->Send(stack_endpoint->GetId(), 1);
  EXPECT_EQ(count, 6);
}

TEST(MessageStackTest, HandlersSkipMissingHandler) {
  int count = 0;
  MessageStackHandlers handlers_1;
  handlers_1.SetHandler<int>(
      [&count](MessageEndpointId from, const int& message) {
        count += 1;
        return false;
      });
  MessageStackHandlers handlers_2;
  MessageStackHandlers handlers_3;
  handlers_3.SetHandler<int>(
      [&count](MessageEndpointId from, const int& message) {
        count += 4;
        return false;
      });
  auto message_system = MessageSystem::Create();
  auto stack_endpoint = MessageStackEndpoint::Create(
      message_system.get(), MessageStackOrder::kTopDown);
  stack_endpoint->Push(&handlers_3);
  stack_endpoint->Push(&handlers_2);
  stack_endpoint->Push(&handlers_1);
  message_system->Send(stack_endpoint->GetId(), 1);
  EXPECT_EQ(count, 5);
}

TEST(MessageStackTest, HandlersSkipDeletedHandler) {
  int count = 0;
  MessageStackHandlers handlers_1;
  handlers_1.SetHandler<int>(
      [&count](MessageEndpointId from, const int& message) {
        count += 1;
        return false;
      });
  auto handlers_2 = std::make_unique<MessageStackHandlers>();
  handlers_2->SetHandler<int>(
      [&count](MessageEndpointId from, const int& message) {
        count += 2;
        return true;
      });
  MessageStackHandlers handlers_3;
  handlers_3.SetHandler<int>(
      [&count](MessageEndpointId from, const int& message) {
        count += 4;
        return false;
      });
  auto message_system = MessageSystem::Create();
  auto stack_endpoint = MessageStackEndpoint::Create(
      message_system.get(), MessageStackOrder::kTopDown);
  stack_endpoint->Push(&handlers_3);
  stack_endpoint->Push(handlers_2.get());
  stack_endpoint->Push(&handlers_1);
  handlers_2.reset();
  message_system->Send(stack_endpoint->GetId(), 1);
  EXPECT_EQ(count, 5);
}

TEST(MessageStackTest, HandlersSkipRemovedHandler) {
  int count = 0;
  MessageStackHandlers handlers_1;
  handlers_1.SetHandler<int>(
      [&count](MessageEndpointId from, const int& message) {
        count += 1;
        return false;
      });
  auto handlers_2 = std::make_unique<MessageStackHandlers>();
  handlers_2->SetHandler<int>(
      [&count](MessageEndpointId from, const int& message) {
        count += 2;
        return true;
      });
  MessageStackHandlers handlers_3;
  handlers_3.SetHandler<int>(
      [&count](MessageEndpointId from, const int& message) {
        count += 4;
        return false;
      });
  auto message_system = MessageSystem::Create();
  auto stack_endpoint = MessageStackEndpoint::Create(
      message_system.get(), MessageStackOrder::kTopDown);
  stack_endpoint->Push(&handlers_3);
  stack_endpoint->Push(handlers_2.get());
  stack_endpoint->Push(&handlers_1);
  stack_endpoint->Remove(handlers_2.get());
  message_system->Send(stack_endpoint->GetId(), 1);
  EXPECT_EQ(count, 5);
}

TEST(MessageStackTest, MultipleMessageTypesWithDifferentOrders) {
  int count = 0;
  MessageStackHandlers handlers_1;
  handlers_1.SetHandler<int>(
      [&count](MessageEndpointId from, const int& message) {
        count += 1;
        return false;
      });
  handlers_1.SetHandler<float>(
      [&count](MessageEndpointId from, const float& message) {
        count += 10;
        return false;
      });
  MessageStackHandlers handlers_2;
  handlers_2.SetHandler<int>(
      [&count](MessageEndpointId from, const int& message) {
        count += 2;
        return true;
      });
  handlers_2.SetHandler<float>(
      [&count](MessageEndpointId from, const float& message) {
        count += 20;
        return true;
      });
  MessageStackHandlers handlers_3;
  handlers_3.SetHandler<int>(
      [&count](MessageEndpointId from, const int& message) {
        count += 4;
        return false;
      });
  handlers_3.SetHandler<float>(
      [&count](MessageEndpointId from, const float& message) {
        count += 40;
        return false;
      });
  auto message_system = MessageSystem::Create();
  auto stack_endpoint = MessageStackEndpoint::Create(
      message_system.get(), MessageStackOrder::kTopDown);
  stack_endpoint->SetOrder<float>(MessageStackOrder::kBottomUp);
  stack_endpoint->Push(&handlers_3);
  stack_endpoint->Push(&handlers_2);
  stack_endpoint->Push(&handlers_1);
  message_system->Send(stack_endpoint->GetId(), 1);
  EXPECT_EQ(count, 3);
  message_system->Send(stack_endpoint->GetId(), 1.0f);
  EXPECT_EQ(count, 63);
}

TEST(MessageStackTest, ClearHandler) {
  int count = 0;
  MessageStackHandlers handlers_1;
  handlers_1.SetHandler<int>(
      [&count](MessageEndpointId from, const int& message) {
        count += 1;
        return true;
      });
  MessageStackHandlers handlers_2;
  handlers_2.SetHandler<int>(
      [&count](MessageEndpointId from, const int& message) {
        count += 2;
        return true;
      });
  auto message_system = MessageSystem::Create();
  auto stack_endpoint = MessageStackEndpoint::Create(
      message_system.get(), MessageStackOrder::kTopDown);
  stack_endpoint->Push(&handlers_2);
  stack_endpoint->Push(&handlers_1);
  handlers_1.ClearHandler<int>();
  message_system->Send(stack_endpoint->GetId(), 1);
  EXPECT_EQ(count, 2);
}

TEST(MessageStackTest, ClearHandlerInsideHandler) {
  int count = 0;
  MessageStackHandlers handlers;
  handlers.SetHandler<int>(
      [&count, &handlers](MessageEndpointId from, const int& message) {
        count += 1;
        handlers.ClearHandler<int>();
        return true;
      });
  auto message_system = MessageSystem::Create();
  auto stack_endpoint = MessageStackEndpoint::Create(
      message_system.get(), MessageStackOrder::kTopDown);
  stack_endpoint->Push(&handlers);
  message_system->Send(stack_endpoint->GetId(), 1);
  EXPECT_EQ(count, 1);
  message_system->Send(stack_endpoint->GetId(), 1);
  EXPECT_EQ(count, 1);
}

}  // namespace
}  // namespace gb
