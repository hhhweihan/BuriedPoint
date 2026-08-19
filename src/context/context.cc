#include "context/context.h"

namespace buried {

void Context::Start() {
  if (is_start_.load()) {
    return;
  }
  is_start_.store(true);
  main_thread_ =
      std::make_unique<std::thread>([this]() { main_context_.run(); });
  report_thread_ =
      std::make_unique<std::thread>([this]() { report_context_.run(); });
}

Context::~Context() {
  main_work_.reset();
  report_work_.reset();
  if (main_thread_ && main_thread_->joinable()) {
    main_thread_->join();
  }
  if (report_thread_ && report_thread_->joinable()) {
    report_thread_->join();
  }
}

}  // namespace buried
