// ============================================================================
//
// = LIBRARY
//    ULib - c++ library
//
// = FILENAME
//    timer.h
//
// = AUTHOR
//    Stefano Casazza
//
// ============================================================================

#ifndef ULIB_TIMER_H
#define ULIB_TIMER_H

#include <ulib/event/event_time.h>
#include <ulib/utility/interrupt.h>

class UNotifier;

// UNotifier use this class to notify a timeout from select()

class U_EXPORT UTimer {
public:

   // Check for memory error
   U_MEMORY_TEST

   // Allocator e Deallocator
   U_MEMORY_ALLOCATOR
   U_MEMORY_DEALLOCATOR

   enum Type { SYNC, ASYNC, NOSIGNAL };

   // COSTRUTTORI

   UTimer()
      {
      U_TRACE_REGISTER_OBJECT(0, UTimer, "", 0)

      next  = 0;
      alarm = 0;
      }

   ~UTimer()
      {
      U_TRACE_UNREGISTER_OBJECT(0, UTimer)

      if (next)  delete next;
      if (alarm) delete alarm;
      }

   // SERVICES

   static bool empty()
      {
      U_TRACE_NO_PARAM(0, "UTimer::empty()")

      if (first == 0) U_RETURN(true);

      U_RETURN(false);
      }

   static bool isAlarm()
      {
      U_TRACE_NO_PARAM(0, "UTimer::isAlarm()")

      if (UInterrupt::timerval.it_value.tv_sec  != 0 ||
          UInterrupt::timerval.it_value.tv_usec != 0)
         {
         U_RETURN(true);
         }

      U_RETURN(false);
      }

   static void clear();                    // cancel all timers and free storage, usually in preparation for exitting
   static void insert(UEventTime* alarm);  // set up a timer, either periodic or one-shot
   static void init(Type mode = NOSIGNAL); // initialize the timer package

   // deschedule a timer. Note that non-periodic timers are automatically descheduled when they run, so you don't have to call this on them

   static void erase(UEventTime* alarm);

   // run the list of timers. Your main program needs to call this every so often, or as indicated by getTimeout()

   static void run();
   static void setTimer();

   static UEventTime* getTimeout() // returns a timeout indicating how long until the next timer triggers
      {
      U_TRACE_NO_PARAM(0, "UTimer::getTimeout()")

      if (        first &&
          (run(), first))
         {
         U_RETURN_POINTER(first->alarm, UEventTime);
         }

      U_RETURN_POINTER(0, UEventTime);
      }

   static bool isHandler(UEventTime* palarm)
      {
      U_TRACE(0, "UTimer::isHandler(%p)", palarm)

      for (UTimer* item = first; item; item = item->next)
         {
         if (item->alarm == palarm) U_RETURN(true);
         }

      U_RETURN(false);
      }

   // manage signal

   static RETSIGTYPE handlerAlarm(int signo)
      {
      U_TRACE(0, "[SIGALRM] UTimer::handlerAlarm(%d)", signo)

      setTimer();
      }

   // STREAM

#ifdef U_STDCPP_ENABLE
   friend U_EXPORT ostream& operator<<(ostream& os, const UTimer& t);

   // DEBUG

# ifdef DEBUG
   static void   printInfo(ostream& os);
          void outputEntry(ostream& os) const U_NO_EXPORT;

   const char* dump(bool reset) const;
# endif
#endif

protected:
   UTimer* next;
   UEventTime* alarm;

   static int mode;
   static UTimer* pool;  //   free list 
   static UTimer* first; // active list 

   static void callHandlerTimeout();

#ifdef DEBUG
   static bool invariant();
#endif

private:
   void insertEntry() U_NO_EXPORT;

   bool operator< (const UTimer& t) const { return (*alarm < *t.alarm); }
   bool operator> (const UTimer& t) const { return  t.operator<(*this); }
   bool operator<=(const UTimer& t) const { return !t.operator<(*this); }
   bool operator>=(const UTimer& t) const { return !  operator<(t); }
   bool operator==(const UTimer& t) const { return (*alarm == *t.alarm); }
   bool operator!=(const UTimer& t) const { return !  operator==(t); }

   friend class UNotifier;

#ifdef U_COMPILER_DELETE_MEMBERS
   UTimer(const UTimer&) = delete;
   UTimer& operator=(const UTimer&) = delete;
#else
   UTimer(const UTimer&)            {}
   UTimer& operator=(const UTimer&) { return *this; }
#endif
};

#endif
