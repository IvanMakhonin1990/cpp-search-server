#include "log_duration.h"

using namespace std;
using Clock = std::chrono::steady_clock;

LogDuration::LogDuration(string _operation_name, ostream &_out_stream)
    : start_time_(Clock::now()), operation_name(_operation_name),
      out_stream(_out_stream) {}

LogDuration::~LogDuration() {
  using namespace std::chrono;
  using namespace std::literals;

  const auto end_time = Clock::now();
  const auto dur = end_time - start_time_;
  std::cerr << operation_name << ": "s
            << duration_cast<milliseconds>(dur).count() << " ms"s << std::endl;
}