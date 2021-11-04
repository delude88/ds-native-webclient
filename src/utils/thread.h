#pragma once

static void set_realtime_prio(std::thread &thread) {
#ifdef _WIN32
  SetThreadPriority(thread, THREAD_PRIORITY_TIME_CRITICAL);
#else
#ifdef SCHED_FIFO
  pthread_attr_t attr;
  if (pthread_attr_init(&attr) == 0) {
    int scheduler = -1;
    if (pthread_attr_setschedpolicy(&attr, SCHED_FIFO) == 0) {
      scheduler = SCHED_FIFO;
    }
    if (scheduler != -1) {
      sched_param param;
      int priorityMax = sched_get_priority_max(scheduler);
      param.sched_priority = priorityMax - 1;
      pthread_setschedparam(thread.native_handle(), SCHED_FIFO, &param);
    }
  }
#endif
#endif
}
