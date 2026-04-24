#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <stdexcept>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sensor_fusion {

template <typename T>
class Topic {
 public:
  using Callback = std::function<void(const T&)>;

  uint64_t subscribe(Callback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    const uint64_t id = next_subscription_id_++;
    subscribers_.emplace_back(id, std::move(cb));
    return id;
  }

  void unsubscribe(uint64_t id) {
    std::lock_guard<std::mutex> lock(mutex_);
    subscribers_.erase(
        std::remove_if(subscribers_.begin(), subscribers_.end(),
                       [id](const auto& entry) { return entry.first == id; }),
        subscribers_.end());
  }

  void publish(const T& msg) {
    std::vector<Callback> callbacks;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      callbacks.reserve(subscribers_.size());
      for (const auto& [subscription_id, callback] : subscribers_) {
        (void)subscription_id;
        callbacks.push_back(callback);
      }
    }

    for (const auto& callback : callbacks) {
      callback(msg);
    }
  }

 private:
  std::mutex mutex_;
  uint64_t next_subscription_id_{1};
  std::vector<std::pair<uint64_t, Callback>> subscribers_;
};

class MessageBus {
 public:
  MessageBus() = default;

  template <typename T>
  Topic<T>& topic(const std::string& name) {
    std::shared_ptr<TopicHolderBase> holder;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      const auto it = topics_.find(name);
      if (it == topics_.end()) {
        auto new_holder = std::make_shared<TopicHolder<T>>();
        holder = new_holder;
        topics_.emplace(name, std::move(new_holder));
      } else {
        holder = it->second;
      }
    }

    if (holder->type() != std::type_index(typeid(T))) {
      throw std::runtime_error("topic type mismatch for name: " + name);
    }

    return static_cast<TopicHolder<T>&>(*holder).topic;
  }

 private:
  struct TopicHolderBase {
    virtual ~TopicHolderBase();
    virtual std::type_index type() const = 0;
  };

  template <typename T>
  struct TopicHolder final : TopicHolderBase {
    Topic<T> topic;

    std::type_index type() const override { return std::type_index(typeid(T)); }
  };

  std::mutex mutex_;
  std::unordered_map<std::string, std::shared_ptr<TopicHolderBase>> topics_;
};

}  // namespace sensor_fusion
