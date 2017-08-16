#ifndef RTOS_LOGGER_H
#define RTOS_LOGGER_H

#include <string>
#include "mbed.h"
#include "mbed_events.h"
#include "rtos.h"

#define RTOS_LOGGER_DEFAULT_STACK_SIZE 1024
#define RTOS_LOGGER_DEFAULT_QUEUE_SIZE 128
#define RTOS_LOGGER_DEFAULT_TIMEZONE_OFFSET 0
#define RTOS_LOGGER_DEFAULT_FAST_UART true

#if defined(RTOS_LOGGER_USE_USBSERIAL)
#include "USBSerial.h"
#endif

#ifndef RTOS_LOGGER_LOCK_ENABLED
#define RTOS_LOGGER_LOCK_ENABLED 0
#endif

class RtosLogger {
 public:
  enum RtosLoggerError { Ok = 0, RTOSError };

  RtosLogger(int stack_size = RTOS_LOGGER_DEFAULT_STACK_SIZE, int queue_size = RTOS_LOGGER_DEFAULT_QUEUE_SIZE,
             bool fast_uart = RTOS_LOGGER_DEFAULT_FAST_UART, int timezone_offset = RTOS_LOGGER_DEFAULT_TIMEZONE_OFFSET);
  ~RtosLogger();

  /**
  * Starts the logger thread, called from DMBoard::instance().init()
  */
  RtosLoggerError init();

  /** Printf that works in an RTOS
  *
  *  This function will create a string from the specified format and
  *  optional extra arguments and put it into a message that the RtosLog
  *  thread will write to the log.
  *
  *  Note that if the underlying queue is full then this function
  *  will block until an entry becomes available. This is required to
  *  make sure that all printf calls actually get printed. If this happens
  *  too often then increase the priority of the RtosLog thread or reduce
  *  the number of printed messages.
  *
  *  @param format  format string
  *  @param ...     optional extra arguments
  */
  int printf(const char *format, ...);

  int printf_time(const char *format, ...);

  /** Blocking printf that uses a mutex to achieve thread synchronization
  *
  *  @param format  format string
  *  @param ...     optional extra arguments
  */
  void print_message_locked(const char *format, ...);

  EventQueue &log_queue() { return _log_queue; }

 private:
  struct LogMessage {
    std::string *message;
  };

#if defined(RTOS_LOGGER_USE_USBSERIAL)
  USBSerial _serial;
#else
  Serial _serial;
#endif

  Thread _logger_thread;
  int _stack_size;
  bool _fast_uart;
  bool _lock;
  int _timezone_offset;
  int _queue_size;
  EventQueue _log_queue;

  void logger_task();

  static Timer s_timer;
  static Mutex s_stdio_mutex;

  static void print_message(RtosLogger *logger, std::string &message);
};

#endif
