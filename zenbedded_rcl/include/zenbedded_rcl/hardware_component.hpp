// Copyright 2026 Open Source Robotics Foundation, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef ZENBEDDED_RCL__HARDWARE_COMPONENT_HPP_
#define ZENBEDDED_RCL__HARDWARE_COMPONENT_HPP_

#include <zephyr/kernel.h>
#include <cstdint>
#include <string_view>

enum class HwReturn : uint8_t
{
  OK = 0,
  ERROR = 1,
  BUSY = 2,
  TIMEOUT = 3,
};

enum class HwState : uint8_t
{
  UNCONFIGURED = 0,
  INACTIVE = 1,
  ACTIVE = 2,
  FINALIZED = 3,
};

struct StateInterface
{
  std::string_view name;

  double get()
  {
    k_mutex_lock(&mutex, K_FOREVER);
    double v = value;
    k_mutex_unlock(&mutex);
    return v;
  }

  void set(double v)
  {
    k_mutex_lock(&mutex, K_FOREVER);
    value = v;
    k_mutex_unlock(&mutex);
  }

  explicit StateInterface(std::string_view n) : name(n) { k_mutex_init(&mutex); }

private:
  double value{0.0};
  struct k_mutex mutex;
};

struct CommandInterface
{
  std::string_view name;

  double get()
  {
    k_mutex_lock(&mutex, K_FOREVER);
    double v = value;
    k_mutex_unlock(&mutex);
    return v;
  }

  void set(double v)
  {
    k_mutex_lock(&mutex, K_FOREVER);
    value = v;
    k_mutex_unlock(&mutex);
  }

  explicit CommandInterface(std::string_view n) : name(n) { k_mutex_init(&mutex); }

private:
  double value{0.0};
  struct k_mutex mutex;
};

class HardwareComponent
{
public:
  explicit HardwareComponent(std::string_view name) : name_(name)
  {
    k_mutex_init(&lifecycle_mutex_);
  }

  virtual ~HardwareComponent() = default;

  HardwareComponent(const HardwareComponent &) = delete;
  HardwareComponent & operator=(const HardwareComponent &) = delete;

  HwReturn configure()
  {
    k_mutex_lock(&lifecycle_mutex_, K_FOREVER);
    HwReturn ret = HwReturn::ERROR;
    if (state_ == HwState::UNCONFIGURED)
    {
      ret = on_configure();
      if (ret == HwReturn::OK)
      {
        state_ = HwState::INACTIVE;
      }
    }
    k_mutex_unlock(&lifecycle_mutex_);
    return ret;
  }

  HwReturn activate()
  {
    k_mutex_lock(&lifecycle_mutex_, K_FOREVER);
    HwReturn ret = HwReturn::ERROR;
    if (state_ == HwState::INACTIVE)
    {
      ret = on_activate();
      if (ret == HwReturn::OK)
      {
        state_ = HwState::ACTIVE;
      }
    }
    k_mutex_unlock(&lifecycle_mutex_);
    return ret;
  }

  HwReturn deactivate()
  {
    k_mutex_lock(&lifecycle_mutex_, K_FOREVER);
    HwReturn ret = HwReturn::ERROR;
    if (state_ == HwState::ACTIVE)
    {
      ret = on_deactivate();
      if (ret == HwReturn::OK)
      {
        state_ = HwState::INACTIVE;
      }
    }
    k_mutex_unlock(&lifecycle_mutex_);
    return ret;
  }

  HwReturn cleanup()
  {
    k_mutex_lock(&lifecycle_mutex_, K_FOREVER);
    HwReturn ret = on_cleanup();
    if (ret == HwReturn::OK)
    {
      state_ = HwState::FINALIZED;
    }
    k_mutex_unlock(&lifecycle_mutex_);
    return ret;
  }

  HwReturn read()
  {
    if (state_ != HwState::ACTIVE)
    {
      return HwReturn::ERROR;
    }
    return on_read();
  }

  HwReturn write()
  {
    if (state_ != HwState::ACTIVE)
    {
      return HwReturn::ERROR;
    }
    return on_write();
  }

  HwState state() const { return state_; }
  std::string_view name() const { return name_; }

protected:
  virtual HwReturn on_configure() { return HwReturn::OK; }
  virtual HwReturn on_activate() { return HwReturn::OK; }
  virtual HwReturn on_deactivate() { return HwReturn::OK; }
  virtual HwReturn on_cleanup() { return HwReturn::OK; }
  virtual HwReturn on_read() = 0;
  virtual HwReturn on_write() = 0;

  StateInterface * state_interfaces_{nullptr};
  CommandInterface * command_interfaces_{nullptr};
  uint8_t n_state_{0};
  uint8_t n_command_{0};

private:
  std::string_view name_;
  HwState state_{HwState::UNCONFIGURED};
  struct k_mutex lifecycle_mutex_;
};

#endif  // ZENBEDDED_RCL__HARDWARE_COMPONENT_HPP_
