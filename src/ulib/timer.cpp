// ============================================================================
//
// = LIBRARY
//    ULib - c++ library
//
// = FILENAME
//    timer.cpp
//
// = AUTHOR
//    Stefano Casazza
//
// ============================================================================

#include <ulib/timer.h>

int     UTimer::mode;
UTimer* UTimer::pool;
UTimer* UTimer::first;

void UTimer::init(Type _mode)
{
   U_TRACE(0, "UTimer::init(%d)", _mode)

   if (u_start_time     == 0 &&
       u_setStartTime() == false)
      {
      U_ERROR("UTimer::init(%d): system date not updated", _mode);
      }

   mode = _mode;

        if (_mode ==  SYNC) UInterrupt::setHandlerForSignal(SIGALRM, (sighandler_t)UTimer::handlerAlarm);
   else if (_mode == ASYNC) UInterrupt::insert(             SIGALRM, (sighandler_t)UTimer::handlerAlarm); // async signal

#ifdef USE_LIBEVENT
   if (u_ev_base == 0) u_ev_base = (struct event_base*) U_SYSCALL_NO_PARAM(event_init);

   U_INTERNAL_ASSERT_POINTER(u_ev_base)
#endif
}

U_NO_EXPORT void UTimer::insertEntry()
{
   U_TRACE_NO_PARAM(0, "UTimer::insertEntry()")

   U_CHECK_MEMORY

   alarm->setCurrentTime();

   if (first == 0) // The list is empty
      {
      next  = 0;
      first = this;
      }
   else
      {
      UTimer** ptr = &first;

      do {
         if (*this < **ptr) break;

         ptr = &(*ptr)->next;
         }
      while (*ptr);

      next = *ptr;
      *ptr = this;
      }

   U_ASSERT(invariant())
}

void UTimer::callHandlerTimeout()
{
   U_TRACE_NO_PARAM(0, "UTimer::callHandlerTimeout()")

   U_INTERNAL_ASSERT_POINTER(first)

   UTimer* item = first;
                  first = first->next; // remove it from its active list

   U_INTERNAL_DUMP("UEventTime::timeout1 = %#19D (next alarm expire) = %#19D", UEventTime::timeout1.tv_sec, item->next ? item->next->alarm->expire() : 0L)

   if (item->alarm->handlerTime())
      {
      // put it on the free list

      item->alarm = 0;
      item->next  = pool;
            pool  = item;
      }
   else // 0 => monitoring
      {
      // add it back in to its new list, sorted correctly

      item->insertEntry();

      UEventTime* a = item->alarm;

      UEventTime::ms = ((a->ctime.tv_sec  + a->tv_sec  - UEventTime::timeout1.tv_sec)  * 1000L) +
                       ((a->ctime.tv_usec + a->tv_usec - UEventTime::timeout1.tv_usec) / 1000L);

      U_ASSERT(a->checkTolerance())
      }
}

void UTimer::run()
{
   U_TRACE_NO_PARAM(1, "UTimer::run()")

   (void) U_SYSCALL(gettimeofday, "%p,%p", &UEventTime::timeout1, 0);

   U_INTERNAL_DUMP("UEventTime::timeout1 = { %ld %6ld } first = %p", UEventTime::timeout1.tv_sec, UEventTime::timeout1.tv_usec, first)

   UTimer* item = first;
   bool bnosignal = (mode == NOSIGNAL);

loop:
#ifdef DEBUG
   U_INTERNAL_DUMP("item = %p item->next = %p", item, item->next)

   U_INTERNAL_ASSERT_POINTER(item)

   if (item->next) U_INTERNAL_ASSERT(*item <= *(item->next))
#endif

   if (bnosignal ? item->alarm->isExpired()
                 : item->alarm->isExpiredWithTolerance())
      {
      item = item->next;

      callHandlerTimeout();

      if (item) goto loop;
      }

   U_INTERNAL_DUMP("first = %p", first)
}

void UTimer::setTimer()
{
   U_TRACE_NO_PARAM(1, "UTimer::setTimer()")

   U_INTERNAL_ASSERT_DIFFERS(mode, NOSIGNAL)

   run();

   if (first == 0)
      {
      UInterrupt::timerval.it_value.tv_sec =
      UInterrupt::timerval.it_value.tv_usec = 0L;
      }
   else
      {
      UEventTime* item = first->alarm;

      UInterrupt::timerval.it_value.tv_sec  = item->ctime.tv_sec  + item->tv_sec  - UEventTime::timeout1.tv_sec;
      UInterrupt::timerval.it_value.tv_usec = item->ctime.tv_usec + item->tv_usec - UEventTime::timeout1.tv_usec;

      UTimeVal::adjust(&(UInterrupt::timerval.it_value.tv_sec),
                       &(UInterrupt::timerval.it_value.tv_usec));
      }

   // NB: it can happen that setitimer() produce immediatly a signal because the interval is very short (< 10ms)... 

   U_INTERNAL_DUMP("UInterrupt::timerval.it_value = { %ld %6ld }", UInterrupt::timerval.it_value.tv_sec, UInterrupt::timerval.it_value.tv_usec)

   U_INTERNAL_ASSERT(UInterrupt::timerval.it_value.tv_sec  >= 0 &&
                     UInterrupt::timerval.it_value.tv_usec >= 0)

   (void) U_SYSCALL(setitimer, "%d,%p,%p", ITIMER_REAL, &UInterrupt::timerval, 0);
}

void UTimer::insert(UEventTime* a)
{
   U_TRACE(0, "UTimer::insert(%p,%b)", a)

   // set an alarm to more than 2 month is very suspect...

   U_INTERNAL_ASSERT_MINOR(a->tv_sec, 60L * U_ONE_DAY_IN_SECOND) // 60 gg (2 month)

   UTimer* item;

   if (pool == 0) item = U_NEW(UTimer);
   else
      {
      item = pool;
      pool = pool->next;
      }

   item->alarm = a;

   // add it in to its new list, sorted correctly

   item->insertEntry();
}

void UTimer::erase(UEventTime* a)
{
   U_TRACE(0, "UTimer::erase(%p)", a)

   U_INTERNAL_ASSERT_POINTER(first)

   UTimer* item;

   for (UTimer** ptr = &first; (item = *ptr); ptr = &(*ptr)->next)
      {
      if (item->alarm == a)
         {
         *ptr = item->next; // remove it from its active list

         U_ASSERT(invariant())

         // and we put it on the free list

         item->alarm = 0;
         item->next  = pool;
                pool = item;

         break;
         }
      }
}

void UTimer::clear()
{
   U_TRACE_NO_PARAM(1, "UTimer::clear()")

   U_INTERNAL_DUMP("mode = %d first = %p pool = %p", mode, first, pool)

   if (mode != NOSIGNAL)
      {
      UInterrupt::timerval.it_value.tv_sec  =
      UInterrupt::timerval.it_value.tv_usec = 0L;

      (void) U_SYSCALL(setitimer, "%d,%p,%p", ITIMER_REAL, &UInterrupt::timerval, 0);
      }

   UTimer* item = first;

   if (first)
      {
      do { item->alarm = 0; } while ((item = item->next));

      delete first;
             first = 0;
      }

   if (pool)
      {
      item = pool;

      do { item->alarm = 0; } while ((item = item->next));

      delete pool;
             pool = 0;
      }
}

#ifdef DEBUG
bool UTimer::invariant()
{
   U_TRACE_NO_PARAM(0, "UTimer::invariant()")

   if (first)
      {
      for (UTimer* item = first; item->next; item = item->next)
         {
         if (item->next) U_INTERNAL_ASSERT(*item <= *(item->next))
         }
      }

   U_RETURN(true);
}
#endif

// STREAM

#ifdef U_STDCPP_ENABLE
U_EXPORT ostream& operator<<(ostream& os, const UTimer& t)
{
   U_TRACE(0+256, "UTimer::operator<<(%p,%p)", &os, &t)

   os.put('(');
   os.put(' ');

   if (t.alarm) os << *t.alarm;
   else         os << (void*)&t;

   for (UTimer* item = t.next; item; item = item->next)
      {
      os.put(' ');

      if (item->alarm) os << *item->alarm;
      else             os << (void*)item;
      }

   os.put(' ');
   os.put(')');

   return os;
}

#  ifdef DEBUG
void UTimer::printInfo(ostream& os)
{
   U_TRACE(0+256, "UTimer::printInfo(%p)", &os)

   os << "first = ";

   if (first) os << *first;
   else       os << (void*)first;

   os << "\npool  = ";

   if (pool) os << *pool;
   else      os << (void*)pool;

   os << "\n";
}

const char* UTimer::dump(bool reset) const
{
   *UObjectIO::os << "timerval                 " << "{ { "  << UInterrupt::timerval.it_interval.tv_sec
                                                 << " "     << UInterrupt::timerval.it_interval.tv_usec
                                                 << " } { " << UInterrupt::timerval.it_value.tv_sec
                                                 << " "     << UInterrupt::timerval.it_value.tv_usec
                                                                 << " } }\n"
                  << "pool         (UTimer     " << (void*)pool  << ")\n"
                  << "first        (UTimer     " << (void*)first << ")\n"
                  << "next         (UTimer     " << (void*)next  << ")\n"
                  << "alarm        (UEventTime " << (void*)alarm << ")";

   if (reset)
      {
      UObjectIO::output();

      return UObjectIO::buffer_output;
      }

   return 0;
}
#  endif
#endif
