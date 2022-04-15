#pragma once
#include <chrono>
#include <iostream>
#include <string>

#define PROFILE_CONCAT_INTERNAL(X, Y) X##Y
#define PROFILE_CONCAT(X, Y) PROFILE_CONCAT_INTERNAL(X, Y)
#define UNIQUE_VAR_NAME_PROFILE PROFILE_CONCAT(profileGuard, __LINE__)
#define LOG_DURATION(x) LogDuration UNIQUE_VAR_NAME_PROFILE(x)
#define LOG_DURATION_STREAM(x, s) LogDuration UNIQUE_VAR_NAME_PROFILE(x, s)

class LogDuration {
public:
  LogDuration(std::string _operation_name,
              std::ostream &_out_stream = std::cerr);

  ~LogDuration();

private:
  const std::chrono::steady_clock::time_point start_time_ =
      std::chrono::steady_clock::now();
  const std::string operation_name;
  std::ostream &out_stream;
};
#pragma once