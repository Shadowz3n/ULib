// ============================================================================
//
// = LIBRARY
//    ULib - c++ library
//
// = FILENAME
//    dir_walk.h
//
// = AUTHOR
//    Stefano Casazza
//
// ============================================================================

#ifndef ULIB_DIR_WALK_H
#define ULIB_DIR_WALK_H 1

#include <ulib/string.h>

#define U_ALPHABETIC_SORT (qcompare)-1

/**
 * FTW - walks through the directory tree starting from the indicated directory.
 *       For each found entry in the tree, it calls foundFile()
 */

                   class IR;
                   class UFile;
                   class UHTTP;
template <class T> class UTree;
template <class T> class UVector;
template <class T> class UHashMap;
                   class PEC_report;

class U_EXPORT UDirWalk {
public:

   typedef struct dirent_s {
      ino_t         d_ino;
      uint32_t      d_name, d_namlen;
      unsigned char d_type;
   } dirent_s;

   typedef struct dir_s {
      char* free;
      dirent_s* dp;
      uint32_t num, max, pfree, nfree, szfree;
   } dir_s;

   // Check for memory error
   U_MEMORY_TEST

   // Allocator e Deallocator
   U_MEMORY_ALLOCATOR
   U_MEMORY_DEALLOCATOR

   // Costruttori

   UDirWalk(const UString* dir = 0, const char* _filter = 0, uint32_t _filter_len = 0)
      {
      U_TRACE_REGISTER_OBJECT(0, UDirWalk, "%p,%.*S,%u", dir, _filter_len, _filter, _filter_len)

      ctor(dir, _filter, _filter_len);
      }

   UDirWalk(const UString* dir, const UString& _filter)
      {
      U_TRACE_REGISTER_OBJECT(0, UDirWalk, "%V,%V", dir->rep, _filter.rep)

      ctor(dir, U_STRING_TO_PARAM(_filter));
      }

   UDirWalk(const UString& dir, const char* _filter = 0, uint32_t _filter_len = 0)
      {
      U_TRACE_REGISTER_OBJECT(0, UDirWalk, "%V,%.*S,%u", dir.rep, _filter_len, _filter, _filter_len)

      ctor(&dir, _filter, _filter_len);
      }

   virtual ~UDirWalk()
      {
      U_TRACE_UNREGISTER_OBJECT(0, UDirWalk)

      if (suffix_file_type)
         {
         delete suffix_file_type;
                suffix_file_type = 0;
         }
      }

   // Begin the journey

   void     walk();
   uint32_t walk(UTree<UString>& tree);
   uint32_t walk(UVector<UString>& vec, qcompare compare_obj = 0);

   // SERVICES

   static bool isDirectory()
      {
      U_TRACE_NO_PARAM(0, "UDirWalk::isDirectory()")

      U_INTERNAL_ASSERT_POINTER(pthis)

      U_RETURN(pthis->is_directory);
      }

   static void setSortingForInode()
      {
      U_TRACE_NO_PARAM(0, "UDirWalk::setSortingForInode()")

      sort_by = cmp_inode;
      }

   static void setFoundFile(UString& path)
      {
      U_TRACE(0, "UDirWalk::setFoundFile(%V)", path.rep)

      U_INTERNAL_ASSERT_POINTER(pthis)

      U_INTERNAL_DUMP("depth = %d pathlen = %u pathname(%u) = %S",
                       pthis->depth, pthis->pathlen, u__strlen(pthis->pathname, __PRETTY_FUNCTION__), pthis->pathname)

      path.replace(pthis->pathname+2, pthis->pathlen-2);
      }

   static void setFollowLinks(bool _bfollowlinks = true)
      {
      U_TRACE(0, "UDirWalk::setFollowLinks(%b)", _bfollowlinks)

      bfollowlinks = _bfollowlinks;
      }

   static void setRecurseSubDirs(bool bcall_if_directory = true)
      {
      U_TRACE(0, "UDirWalk::setRecurseSubDirs(%b)", bcall_if_directory)

      brecurse          = true;
      call_if_directory = bcall_if_directory;
      }

   static void setSuffixFileType(const char* format, ...);

   static void setSuffixFileType(const char* str, uint32_t len)
      {
      U_TRACE(0, "UDirWalk::setSuffixFileType(%.*S,%u)", len, str, len)

      U_INTERNAL_ASSERT_EQUALS(suffix_file_type, 0)

      suffix_file_type = U_NEW(UString(str, len));
      }

   static void setFilter(const char* _filter, uint32_t _filter_len)
      {
      U_TRACE(0, "UDirWalk::setFilter(%.*S,%u)", _filter_len, _filter, _filter_len)

                      filter     = _filter;
      bfollowlinks = (filter_len = _filter_len);
      }

   static bool setDirectory(const UString& dir, const char* filter = 0, uint32_t filter_len = 0);

   static void setFilter( const UString& _filter)                       { setFilter(U_STRING_TO_PARAM(_filter)); }
   static void setDirectory(const UString& dir, const UString& _filter) { setDirectory(dir, U_STRING_TO_PARAM(_filter)); }

   // DEBUG

#if defined(U_STDCPP_ENABLE) && defined(DEBUG)
   const char* dump(bool reset) const;
#endif

protected:
   int depth;
   uint32_t pathlen;
   bool is_directory;
   char pathname[4000];

   static UDirWalk* pthis;
   static qcompare sort_by;
   static const char* filter;
   static UTree<UString>* ptree;
   static uint32_t max, filter_len;
   static UString* suffix_file_type;
   static UVector<UString>* pvector;
   static vPF call_if_up, call_internal;
   static UHashMap<UFile*>* cache_file_for_compare;
   static bool tree_root, call_if_directory, bfollowlinks, brecurse; // recurse subdirectories?

   void ctor(const UString* dir, const char* filter, uint32_t filter_len);

   // foundFile() is called whenever another file or directory is
   // found that meets the criteria in effect for the object. This
   // can be overridden in derived classes

   virtual void foundFile()
      {
      U_TRACE_NO_PARAM(0, "UDirWalk::foundFile()")

      U_INTERNAL_ASSERT_EQUALS(pthis, this)

      if (call_internal) call_internal();
      }

   static int cmp_inode(const void* a, const void* b) { return (((const dirent_s*)a)->d_ino - ((const dirent_s*)b)->d_ino); }

private:
   bool isFile() U_NO_EXPORT;
   void recurse() U_NO_EXPORT; // performs the actual work
   void prepareForCallingRecurse(char* d_name, uint32_t d_namlen, unsigned char d_type) U_NO_EXPORT;

   static void treeUp() U_NO_EXPORT;
   static void treePush() U_NO_EXPORT;
   static void vectorPush() U_NO_EXPORT;

   static int cmp_modify(const void* a, const void* b) U_NO_EXPORT;

#ifdef U_COMPILER_DELETE_MEMBERS
   UDirWalk(const UDirWalk&) = delete;
   UDirWalk& operator=(const UDirWalk&) = delete;
#else
   UDirWalk(const UDirWalk&)            {}
   UDirWalk& operator=(const UDirWalk&) { return *this; }
#endif      

   friend class IR;
   friend class UHTTP;
   friend class PEC_report;
};
#endif
