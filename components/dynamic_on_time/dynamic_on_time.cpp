// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2023 Ilia Sotnikov

#include "dynamic_on_time.h"  // NOLINT(build/include_subdir)
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace dynamic_on_time {

static const char *tag = "dynamic_on_time";

DynamicOnTime::DynamicOnTime(
  time::RealTimeClock *rtc,
  number::Number *hour,
  number::Number *minute,
  switch_::Switch *mon,
  switch_::Switch *tue,
  switch_::Switch *wed,
  switch_::Switch *thu,
  switch_::Switch *fri,
  switch_::Switch *sat,
  switch_::Switch *sun,
  std::vector<esphome::Action<> *> actions):
    rtc_(rtc),
    hour_(hour), minute_(minute),
    mon_(mon), tue_(tue), wed_(wed), thu_(thu), fri_(fri), sat_(sat),
    sun_(sun), actions_(actions) {}

std::vector<uint8_t> DynamicOnTime::flags_to_days_of_week_(
  bool mon, bool tue, bool wed, bool thu, bool fri, bool sat, bool sun
) {
  // Numeric representation for days of week (starts from Sun internally)
  std::vector<uint8_t> days_of_week = { 1, 2, 3, 4, 5, 6, 7 };
  std::vector<bool> flags = { sun, mon, tue, wed, thu, fri, sat };

  // Translate set of bool flags into vector of corresponding numeric
  // representation. This uses 'erase-remove' approach (
  // https://en.cppreference.com/w/cpp/algorithm/remove,
  // https://en.wikipedia.org/wiki/Erase%E2%80%93remove_idiom)
  days_of_week.erase(
    std::remove_if(
      std::begin(days_of_week), std::end(days_of_week),
      [&](uint8_t& arg) { return !flags[&arg - days_of_week.data()]; }),
    days_of_week.end());

  return days_of_week;
}

void DynamicOnTime::setup() {
  // Update the configuration initially, ensuring all entities are created
  // before a callback would be delivered to them
  this->update_schedule_();

  // Register the cron trigger component
  App.register_component(this->trigger_);

  // The `Number` and `Switch` has no common base type with
  // `add_on_state_callback`, and solutions to properly cast to derived
  // class in single loop over vector of base class instances seemingly imply
  // more code than just two loops
  for (number::Number *comp : {this->hour_, this->minute_}) {
    comp->add_on_state_callback([this](float value) {
      this->update_schedule_();
    });
  }

  for (switch_::Switch *comp : {
    this->mon_, this->tue_, this->wed_, this->thu_, this->fri_, this->sat_,
    this->sun_
  }) {
    comp->add_on_state_callback([this](float value) {
      this->update_schedule_();
    });
  }
}

void DynamicOnTime::update_schedule_() {
  // CronTrigger doesn't allow its configuration to be reset programmatically,
  // so its instance is either created initially, or reinitialized in place if
  // allocated already
  if (this->trigger_ != nullptr) {
    // Use 'placement new' (https://en.cppreference.com/w/cpp/language/new) to
    // reinitialize existing CronTrigger instance in place
    this->trigger_->~CronTrigger();
    new (this->trigger_) time::CronTrigger(this->rtc_);
  } else {
    this->trigger_ = new time::CronTrigger(this->rtc_);
  }

  // (Re)create the automation instance
  if (this->automation_ != nullptr)
    delete this->automation_;
  this->automation_ = new Automation<>(this->trigger_);
  // Add requested actions to it
  this->automation_->add_actions(this->actions_);

  // Set trigger to fire on zeroth second of configured time
  this->trigger_->add_second(0);
  // Enable all days of months for the schedule
  for (uint8_t i = 1; i <= 31; i++)
    this->trigger_->add_day_of_month(i);
  // Same but for months
  for (uint8_t i = 1; i <= 12; i++)
    this->trigger_->add_month(i);
  // Configure hour/minute of the schedule from corresponding components' state
  this->trigger_->add_hour(static_cast<uint8_t>(this->hour_->state));
  this->trigger_->add_minute(static_cast<uint8_t>(this->minute_->state));
  // Similarly but for days of week translating set of components' state to
  // vector of numeric representation as `CrontTrigger::add_days_of_week()`
  // requires
  std::vector<uint8_t> days_of_week = this->flags_to_days_of_week_(
    this->mon_->state, this->tue_->state, this->wed_->state,
    this->thu_->state, this->fri_->state, this->sat_->state,
    this->sun_->state);
  this->trigger_->add_days_of_week(days_of_week);

  // Log the configuration
  this->dump_config();
}

void DynamicOnTime::dump_config() {
  ESP_LOGCONFIG(tag, "Cron trigger details:");
  ESP_LOGCONFIG(
    tag, "Hour (source: '%s'): %.0f",
    this->hour_->get_name().c_str(), this->hour_->state);
  ESP_LOGCONFIG(
    tag, "Minute (source: '%s'): %.0f",
    this->minute_->get_name().c_str(), this->minute_->state);
  ESP_LOGCONFIG(
    tag, "Mon (source: '%s'): %s",
    this->mon_->get_name().c_str(), this->mon_->state ? "Yes": "No");
  ESP_LOGCONFIG(
    tag, "Tue (source: '%s'): %s",
    this->tue_->get_name().c_str(), this->tue_->state ? "Yes": "No");
  ESP_LOGCONFIG(
    tag, "Wed (source: '%s'): %s",
    this->wed_->get_name().c_str(), this->wed_->state ? "Yes": "No");
  ESP_LOGCONFIG(
    tag, "Thu (source: '%s'): %s",
    this->thu_->get_name().c_str(), this->thu_->state ? "Yes": "No");
  ESP_LOGCONFIG(
    tag, "Fri (source: '%s'): %s",
    this->fri_->get_name().c_str(), this->fri_->state ? "Yes": "No");
  ESP_LOGCONFIG(
    tag, "Sat (source: '%s'): %s",
    this->sat_->get_name().c_str(), this->sat_->state ? "Yes": "No");
  ESP_LOGCONFIG(
    tag, "Sun (source: '%s'): %s",
    this->sun_->get_name().c_str(), this->sun_->state ? "Yes": "No");
}

}  // namespace dynamic_on_time
}  // namespace esphome
