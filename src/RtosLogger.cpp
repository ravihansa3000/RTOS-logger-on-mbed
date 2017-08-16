#include "RtosLogger.h"
#include <stdarg.h>
#include <sys/time.h>
#include "mbed.h"
#include "rtos.h"

Timer RtosLogger::s_timer;
Mutex RtosLogger::s_stdio_mutex;

/******************************************************************************
* Private Functions
*****************************************************************************/
void RtosLogger::logger_task() { _log_queue.dispatch_forever(); }

/******************************************************************************
* Public Functions
*****************************************************************************/
#if defined(RTOS_LOGGER_USE_USBSERIAL)
RtosLogger::RtosLogger(int stack_size, int queue_size, bool fast_uart, int timezone_offset)
    : _serial(),
      _logger_thread(osPriorityLow, stack_size, NULL, "RTOS_Logger"),
      _stack_size(stack_size),
      _fast_uart(fast_uart),
      _timezone_offset(timezone_offset),
      _queue_size(queue_size),
      _log_queue(queue_size * EVENTS_EVENT_SIZE, NULL) {
  // TODO: sync RTOS logger Timer with RTC
  s_timer.start();
}
#else
RtosLogger::RtosLogger(int stack_size, int queue_size, bool fast_uart, int timezone_offset)
    : _serial(USBTX, USBRX),
      _logger_thread(osPriorityLow, stack_size, NULL, "RTOS_Logger"),
      _stack_size(stack_size),
      _fast_uart(fast_uart),
      _timezone_offset(timezone_offset),
      _queue_size(queue_size),
      _log_queue(queue_size * EVENTS_EVENT_SIZE, NULL) {
  // TODO: sync RTOS logger Timer with RTC
  s_timer.start();
  if (_fast_uart) {
    // This works because both the default serial (used by printf) and the s instance
    // (used by s.printf) would use the same underlying UART code so setting the baudrate
    // in one affects the other.
    _serial.baud(115200);
  }
}
#endif

RtosLogger::~RtosLogger() { _log_queue.break_dispatch(); }

RtosLogger::RtosLoggerError RtosLogger::init() {
  RtosLoggerError err = Ok;
  osStatus status = _logger_thread.start(callback(this, &RtosLogger::logger_task));
  if (status != osOK) {
    err = RTOSError;
  }
  osThreadId threadId = osThreadGetId();
  this->printf_time(
      "[INFO][RTOS-LOGGER] RTOS logger task thread (TID: %p) started with a stack size of %d, "
      "queue size of %d ... [OK] \r\n",
      threadId, _stack_size, _queue_size);
  return err;
}

int RtosLogger::printf(const char *format, ...) {
  char *ptr = NULL;
  std::va_list args;
  va_start(args, format);
  int ret = vasprintf(&ptr, format, args);
  va_end(args);
  if (ret < 0) {
    free(ptr);
    return ret;
  }

  int id = _log_queue.call(&RtosLogger::print_message, this, std::string(ptr));
  if (id == 0) {
    free(ptr);
    return -1001;
  }

  free(ptr);
  return ret;
}

int RtosLogger::printf_time(const char *format, ...) {
  time_t ctTime = time(NULL) + _timezone_offset;
  char buffer[32], usec_buf[4];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", localtime(&ctTime));
  strcat(buffer, ".");
  snprintf(usec_buf, sizeof(usec_buf), "%03d", s_timer.read_ms() % 1000);
  strcat(buffer, usec_buf);
  strcat(buffer, " ");

  char *ptr = NULL;
  std::va_list args;
  va_start(args, format);
  int ret = vasprintf(&ptr, format, args);
  va_end(args);
  if (ret < 0) {
    free(ptr);
    return ret;
  }

  int id = this->log_queue().call(&RtosLogger::print_message, this, std::string(buffer + std::string(ptr)));
  if (id == 0) {
    free(ptr);
    return -1001;
  }

  free(ptr);
  return ret;
}

void RtosLogger::print_message_locked(const char *format, ...) {
  va_list args;
  va_start(args, format);
  s_stdio_mutex.lock();
  _serial.vprintf(format, args);
  s_stdio_mutex.unlock();
  va_end(args);
}

void RtosLogger::print_message(RtosLogger *logger, std::string &message) {
#if RTOS_LOGGER_LOCK_ENABLED == 1
  logger->s_stdio_mutex.lock();
#endif

  logger->_serial.printf(message.c_str());

#if RTOS_LOGGER_LOCK_ENABLED == 1
  logger->s_stdio_mutex.unlock();
#endif
}
