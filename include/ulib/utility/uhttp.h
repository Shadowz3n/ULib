// ============================================================================
//
// = LIBRARY
//    ULib - c++ library
//
// = FILENAME
//    uhttp.h - HTTP utility
//
// = AUTHOR
//    Stefano Casazza
//
// ============================================================================

#ifndef ULIB_HTTP_H
#define ULIB_HTTP_H 1

#include <ulib/timeval.h>
#include <ulib/internal/chttp.h>
#include <ulib/dynamic/dynamic.h>
#include <ulib/utility/services.h>
#include <ulib/net/server/server.h>
#include <ulib/container/hash_map.h>
#include <ulib/utility/string_ext.h>
#include <ulib/utility/data_session.h>

#if defined(U_ALIAS) && defined(USE_LIBPCRE) // REWRITE RULE
#  include <ulib/pcre/pcre.h>
#else
#  include <ulib/container/vector.h>
#endif

#define U_HTTP_REALM "Protected Area" // HTTP Access Authentication

#define U_MAX_UPLOAD_PROGRESS   16
#define U_MIN_SIZE_FOR_DEFLATE 150 // NB: google advice...

#define U_HTTP_URI_EQUAL(str)               ((str).equal(U_HTTP_URI_TO_PARAM))
#define U_HTTP_URI_DOSMATCH(mask,len,flags) (UServices::dosMatchWithOR(U_HTTP_URI_TO_PARAM, mask, len, flags))

class UFile;
class ULock;
class UEventFd;
class UCommand;
class UPageSpeed;
class USSIPlugIn;
class UHttpPlugIn;
class USSLSession;
class UMimeMultipart;
class UModProxyService;

template <class T> class URDBObjectHandler;

enum EnvironmentType {
   U_CGI   = 0x001,
   U_PHP   = 0x002,
   U_RAKE  = 0x004,
   U_SHELL = 0x008
};

enum DynamicPageType {
   U_DPAGE_INIT    = -1,
   U_DPAGE_RESET   = -2,
   U_DPAGE_DESTROY = -3,
   U_DPAGE_SIGHUP  = -4,
   U_DPAGE_FORK    = -5
};

class U_EXPORT UHTTP {
public:

   // COSTRUTTORE e DISTRUTTORE

   static void init();
   static void dtor();

   static bool readRequest();
   static void setStatusDescription();
   static bool isValidMethod(const char* ptr) __pure;
   static bool scanfHeaderRequest(const char* ptr, uint32_t size);
   static bool scanfHeaderResponse(const char* ptr, uint32_t size);
   static bool readHeaderResponse(USocket* socket, UString& buffer);
   static bool isValidRequest(   const char* ptr, uint32_t size) __pure;
   static bool isValidRequestExt(const char* ptr, uint32_t size) __pure;
   static bool readBodyResponse(USocket* socket, UString* buffer, UString& body);

   // TYPE

   static bool isGET()
      {
      U_TRACE_NO_PARAM(0, "UHTTP::isGET()")

      if (U_http_method_type == HTTP_GET) U_RETURN(true);

      U_RETURN(false);
      }

   static bool isHEAD()
      {
      U_TRACE_NO_PARAM(0, "UHTTP::isHEAD()")

      if (U_http_method_type == HTTP_HEAD) U_RETURN(true);

      U_RETURN(false);
      }

   static bool isPOST()
      {
      U_TRACE_NO_PARAM(0, "UHTTP::isPOST()")

      if (U_http_method_type == HTTP_POST) U_RETURN(true);

      U_RETURN(false);
      }

   static bool isPUT()
      {
      U_TRACE_NO_PARAM(0, "UHTTP::isPUT()")

      if (U_http_method_type == HTTP_PUT) U_RETURN(true);

      U_RETURN(false);
      }

   static bool isPATCH()
      {
      U_TRACE_NO_PARAM(0, "UHTTP::isPATCH()")

      if (U_http_method_type == HTTP_PATCH) U_RETURN(true);

      U_RETURN(false);
      }

   static bool isDELETE()
      {
      U_TRACE_NO_PARAM(0, "UHTTP::isDELETE()")

      if (U_http_method_type == HTTP_DELETE) U_RETURN(true);

      U_RETURN(false);
      }

   static bool isCOPY()
      {
      U_TRACE_NO_PARAM(0, "UHTTP::isCOPY()")

      if (U_http_method_type == HTTP_COPY) U_RETURN(true);

      U_RETURN(false);
      }

   static bool isGETorHEAD()
      {
      U_TRACE_NO_PARAM(0, "UHTTP::isGETorHEAD()")

      if ((U_http_method_type & (HTTP_GET | HTTP_HEAD)) != 0) U_RETURN(true);

      U_RETURN(false);
      }

   static bool isGETorPOST()
      {
      U_TRACE_NO_PARAM(0, "UHTTP::isGETorPOST()")

      if ((U_http_method_type & (HTTP_GET | HTTP_POST)) != 0) U_RETURN(true);

      U_RETURN(false);
      }

   static bool isPOSTorPUTorPATCH()
      {
      U_TRACE_NO_PARAM(0, "UHTTP::isPOSTorPUTorPATCH()")

      if ((U_http_method_type & (HTTP_POST | HTTP_PUT | HTTP_PATCH)) != 0) U_RETURN(true);

      U_RETURN(false);
      }

   static bool isMobile() __pure;
   static bool isProxyRequest();

   static bool isTSARequest() __pure;
   static bool isSOAPRequest() __pure;

   // SERVICES

   static UFile* file;
   static UString* ext;
   static UString* etag;
   static UString* suffix;
   static UString* request;
   static UString* qcontent;
   static UString* pathname;
   static UString* rpathname;
   static UString* mount_point;
   static UString* string_HTTP_Variables;

   static URDB* db_not_found;
   static UModProxyService* service;
   static UVector<UString>* vmsg_error;
   static UVector<UModProxyService*>* vservice;

   static char response_buffer[64];
   static int mime_index, cgi_timeout; // the time-out value in seconds for output cgi process
   static bool enable_caching_by_proxy_servers;
   static uint32_t npathinfo, limit_request_body, request_read_timeout, range_start, range_size, response_code;

   static uint32_t getUserAgent()
      {
      U_TRACE_NO_PARAM(0, "UHTTP::getUserAgent()")

      uint32_t agent = (U_http_info.user_agent_len ? u_cdb_hash((unsigned char*)U_HTTP_USER_AGENT_TO_PARAM, -1) : 0);

      U_RETURN(agent);
      }

   static void setPathName();
   static void checkForPathName()
      {
      U_TRACE_NO_PARAM(0, "UHTTP::checkForPathName()")

      if (pathname->empty())
         {
         setPathName(); 

         file->setPath(*pathname);

         U_INTERNAL_DUMP("file = %.*S", U_FILE_TO_TRACE(*file))
         }
      }

   static void setHostname(const char* ptr, uint32_t len)
      {
      U_TRACE(0, "UHTTP::setHostname(%.*S,%u)", len, ptr, len)

      // The difference between HTTP_HOST and U_HTTP_VHOST is that HTTP_HOST can include the �:PORT� text, and U_HTTP_VHOST only the name

      U_http_host_len  =
      U_http_host_vlen = len;
      U_http_info.host = ptr;

      U_INTERNAL_DUMP("U_http_host_len = %u U_HTTP_HOST = %.*S", U_http_host_len, U_HTTP_HOST_TO_TRACE)

      // hostname[:port]

      for (const char* endptr = ptr+len; ptr < endptr; ++ptr)
         {
         if (*ptr == ':')
            {
            U_http_host_vlen = ptr-U_http_info.host;

            break;
            }
         }

      U_INTERNAL_DUMP("U_http_host_vlen = %u U_HTTP_VHOST = %.*S", U_http_host_vlen, U_HTTP_VHOST_TO_TRACE)
      }

   static int  handlerREAD();
   static bool handlerCache();
   static int  manageRequest();
   static int  processRequest();
   static void initDbNotFound();
   static void setEndRequestProcessing();
   static bool callService(const UString& path);
   static bool isUriRequestNeedCertificate() __pure;
   static bool manageSendfile(const char* ptr, uint32_t len);
   static bool checkContentLength(uint32_t length, uint32_t pos);

   static const char* getStatusDescription(uint32_t* plen = 0);

   static const char* getHeaderValuePtr(                        const char* name, uint32_t name_len, bool nocase) __pure;
   static const char* getHeaderValuePtr(const UString& request, const char* name, uint32_t name_len, bool nocase) __pure;

   static const char* getHeaderValuePtr(                        const UString& name, bool nocase) { return getHeaderValuePtr(         U_STRING_TO_PARAM(name), nocase); }
   static const char* getHeaderValuePtr(const UString& request, const UString& name, bool nocase) { return getHeaderValuePtr(request, U_STRING_TO_PARAM(name), nocase); }

   static UString getHeaderMimeType(const char* content, uint32_t size, const char* content_type, time_t expire = 0L, bool content_length_changeable = false);

   // set HTTP response message

   enum RedirectResponseType {
      NO_BODY                         = 0x001,
      REFRESH                         = 0x002,
      PARAM_DEPENDENCY                = 0x004,
      NETWORK_AUTHENTICATION_REQUIRED = 0x008
   };

   static void setDynamicResponse();
   static void setResponse(const UString* content_type, UString* pbody);
   static void setRedirectResponse(int mode, const char* ptr_location, uint32_t len_location);
   static void setErrorResponse(const UString* content_type, int code, const char* fmt, uint32_t len);

   // set HTTP main error message

   static void setNotFound()
      {
      U_TRACE_NO_PARAM(0, "UHTTP::setNotFound()")

      setErrorResponse(UString::str_ctype_html, HTTP_NOT_FOUND, "Your requested URL %.*S was not found on this server", 0);
      }

   static void setBadMethod()
      {
      U_TRACE_NO_PARAM(0, "UHTTP::setBadMethod()")

      U_INTERNAL_ASSERT_EQUALS(U_http_info.nResponseCode, HTTP_BAD_METHOD)

      setErrorResponse(UString::str_ctype_html, HTTP_BAD_METHOD, "The requested method is not allowed for the URL %.*S", 0);
      }

   static void setBadRequest()
      {
      U_TRACE_NO_PARAM(0, "UHTTP::setBadRequest()")

      UClientImage_Base::resetPipelineAndSetCloseConnection();

      setErrorResponse(UString::str_ctype_html, HTTP_BAD_REQUEST, "Your requested URL %.*S was a request that this server could not understand", 0);
      }

   static void setForbidden()
      {
      U_TRACE_NO_PARAM(0, "UHTTP::setForbidden()")

      UClientImage_Base::setRequestForbidden();

      setErrorResponse(UString::str_ctype_html, HTTP_FORBIDDEN, "You don't have permission to access %.*S on this server", 0);
      }

   static void setInternalError()
      {
      U_TRACE_NO_PARAM(0, "UHTTP::setInternalError()")

      setErrorResponse(UString::str_ctype_html, HTTP_INTERNAL_ERROR,
                       U_CONSTANT_TO_PARAM("The server encountered an internal error or misconfiguration "
                                           "and was unable to complete your request. Please contact the server "
                                           "administrator, and inform them of the time the error occurred, and "
                                           "anything you might have done that may have caused the error. More "
                                           "information about this error may be available in the server error log"));
      }

   static void setServiceUnavailable()
      {
      U_TRACE_NO_PARAM(0, "UHTTP::setServiceUnavailable()")

      setErrorResponse(UString::str_ctype_html, HTTP_UNAVAILABLE,
                       U_CONSTANT_TO_PARAM("Sorry, the service you requested is not available at this moment. "
                                           "Please contact the server administrator and inform them about this"));
      }

   static void setUnAuthorized();

   // HTTP STRICT TRANSPORT SECURITY

#ifdef U_HTTP_STRICT_TRANSPORT_SECURITY
   static UString* uri_strict_transport_security_mask;

   static bool isUriRequestStrictTransportSecurity() __pure;
#endif

   // URI PROTECTION (for example directory listing)

   static UString* htpasswd;
   static UString* htdigest;
   static bool digest_authentication; // authentication method (digest|basic)

   static UString getUserHA1(const UString& user, const UString& realm);
   static bool    isUserAuthorized(const UString& user, const UString& password);

#ifdef USE_LIBSSL
   static UString* uri_protected_mask;
   static UVector<UIPAllow*>* vallow_IP;
   static UString* uri_request_cert_mask;

   static bool checkUriProtected();
   static bool isUriRequestProtected() __pure;
#endif

#ifdef U_ALIAS
   static UString* alias;
   static bool virtual_host;
   static UString* global_alias;
   static UVector<UString>* valias;
   static UString* maintenance_mode_page;

   static void setGlobalAlias(const UString& alias);
#endif

   // manage HTTP request service

   typedef struct service_info {
      const char* name;
      uint32_t    len;
      vPF         function;
   } service_info;

   // NB: the tables must be ordered alphabetically by binary search...

#  define  GET_ENTRY(name) {#name,U_CONSTANT_SIZE(#name), GET_##name}
#  define POST_ENTRY(name) {#name,U_CONSTANT_SIZE(#name),POST_##name}

   static void manageRequest(service_info*  GET_table, uint32_t n1,
                             service_info* POST_table, uint32_t n2);

   // -----------------------------------------------------------------------
   // FORM
   // -----------------------------------------------------------------------
   // retrieve information on specific HTML form elements
   // (such as checkboxes, radio buttons, and text fields), or uploaded files
   // -----------------------------------------------------------------------

   static UString* tmpdir;
   static UMimeMultipart* formMulti;
   static UVector<UString>* form_name_value;

   static uint32_t processForm();
   static void     getFormValue(UString& value, const char* name, uint32_t len);
   static void     getFormValue(UString& value,                                                 uint32_t pos);
   static UString  getFormValue(                const char* name, uint32_t len, uint32_t start,               uint32_t end);
   static void     getFormValue(UString& value, const char* name, uint32_t len, uint32_t start, uint32_t pos, uint32_t end);

   // COOKIE

   static UString* set_cookie;
   static uint32_t sid_counter_gen;
   static UString* set_cookie_option;
   static UString* cgi_cookie_option;

   static bool    getCookie(      UString* cookie);
   static void addSetCookie(const UString& cookie);

   // -----------------------------------------------------------------------------------------------------------------------------------
   // param: "[ data expire path domain secure HttpOnly ]"
   // -----------------------------------------------------------------------------------------------------------------------------------
   // string -- key_id or data to put in cookie    -- must
   // int    -- lifetime of the cookie in HOURS    -- must (0 -> valid until browser exit)
   // string -- path where the cookie can be used  --  opt
   // string -- domain which can read the cookie   --  opt
   // bool   -- secure mode                        --  opt
   // bool   -- only allow HTTP usage              --  opt
   // -----------------------------------------------------------------------------------------------------------------------------------
   // RET: Set-Cookie: ulib.s<counter>=data&expire&HMAC-MD5(data&expire); expires=expire(GMT); path=path; domain=domain; secure; HttpOnly
   // -----------------------------------------------------------------------------------------------------------------------------------

   static void setCookie(const UString& param);

   // HTTP SESSION

   static uint32_t sid_counter_cur;
   static UDataSession* data_session;
   static UDataSession* data_storage;
   static URDBObjectHandler<UDataStorage*>* db_session;

   static void  initSession();
   static void clearSession();
   static void removeDataSession();
   static void setSessionCookie(UString* param = 0);

   static bool getDataStorage();
   static bool getDataSession();
   static bool getDataStorage(uint32_t index, UString& value);
   static bool getDataSession(uint32_t index, UString& value);

   static void putDataStorage();
   static void putDataSession();
   static void putDataStorage(uint32_t index, const char* val, uint32_t sz);
   static void putDataSession(uint32_t index, const char* val, uint32_t sz);

   static bool    isNewSession()               { return data_session->isNewSession(); }
   static bool    isDataSession()              { return data_session->isDataSession(); }
   static UString getSessionCreationTime()     { return data_session->getSessionCreationTime(); }
   static UString getSessionLastAccessedTime() { return data_session->getSessionLastAccessedTime(); }

#ifdef USE_LIBSSL
   static USSLSession* data_session_ssl;
   static URDBObjectHandler<UDataStorage*>* db_session_ssl;

   static void  initSessionSSL();
   static void clearSessionSSL();
#endif

   static UString getKeyIdDataSession()
      {
      U_TRACE_NO_PARAM(0, "UHTTP::getKeyIdDataSession()")

      U_INTERNAL_ASSERT_POINTER(data_session)

      U_RETURN_STRING(data_session->keyid);
      }

   // HTML Pagination

   static uint32_t num_page_end,
                   num_page_cur,
                   num_page_start,
                   num_item_tot,
                   num_item_for_page;

   static UString getLinkPagination();

   static void addLinkPagination(UString& link, uint32_t num_page)
      {
      U_TRACE(0, "UHTTP::addLinkPagination(%V,%u)", link.rep, num_page)

#  ifdef U_HTML_PAGINATION_SUPPORT
      UString x(100U);

      U_INTERNAL_DUMP("num_page_cur = %u", num_page_cur)

      if (num_page == num_page_cur) x.snprintf("<span class=\"pnow\">%u</span>",             num_page);
      else                          x.snprintf("<a href=\"?page=%u\" class=\"pnum\">%u</a>", num_page, num_page);

      (void) link.append(x);
             link.push_back(' ');
#  endif
      }

   // CGI

   typedef struct ucgi {
      const char* interpreter;
      char        environment_type;
      char        dir[503];
   } ucgi;

   static UCommand* pcmd;
   static UString* geoip;
   static UString* fcgi_uri_mask;
   static UString* scgi_uri_mask;

   static bool isFCGIRequest() __pure;
   static bool isSCGIRequest() __pure;

   static bool runCGI(bool set_environment);
   static bool getCGIEnvironment(UString& environment, int mask);
   static bool processCGIOutput(bool cgi_sh_script, bool bheaders);
   static bool processCGIRequest(UCommand* cmd, UHTTP::ucgi* cgi = 0);

   // USP (ULib Servlet Page)

   class UServletPage : public UDynamic {
   public:

   vPFi runDynamicPage;

   // COSTRUTTORI

   UServletPage()
      {
      U_TRACE_REGISTER_OBJECT(0, UServletPage, "", 0)

      runDynamicPage = 0;
      }

   ~UServletPage()
      {
      U_TRACE_UNREGISTER_OBJECT(0, UServletPage)
      }

#if defined(U_STDCPP_ENABLE) && defined(DEBUG)
   const char* dump(bool reset) const U_EXPORT;
#endif

   private:
#ifdef U_COMPILER_DELETE_MEMBERS
   UServletPage(const UServletPage&) = delete;
   UServletPage& operator=(const UServletPage&) = delete;
#else
   UServletPage(const UServletPage&) : UDynamic() {}
   UServletPage& operator=(const UServletPage&)   { return *this; }
#endif      
   };

   static bool bcallInitForAllUSP;
   static const char* usp_page_key;
   static uint32_t usp_page_key_len;
   static UServletPage* usp_page_ptr;

   static bool callEndForAllUSP(UStringRep* key, void* value);
   static bool callInitForAllUSP(UStringRep* key, void* value);
   static bool callSigHUPForAllUSP(UStringRep* key, void* value);
   static bool callAfterForkForAllUSP(UStringRep* key, void* value);

   static UServletPage* getUSP(const char* key, uint32_t key_len);

   // ------------------------------------------------------------------------------------------------------------------------------------------------- 
   // COMMON LOG FORMAT (APACHE LIKE LOG)
   // ------------------------------------------------------------------------------------------------------------------------------------------------- 
   // The Common Log Format, also known as the NCSA Common log format, is a standardized text file format used by web servers
   // when generating server log files. Because the format is standardized, the files may be analyzed by a variety of web analysis programs.
   // Each line in a file stored in the Common Log Format has the following syntax: host ident authuser date request status bytes
   // ------------------------------------------------------------------------------------------------------------------------------------------------- 
   // 10.10.25.2 - - [21/May/2012:16:29:41 +0200] "GET / HTTP/1.1" 200 598 "-" "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/536.5 (KHTML, like Gecko)"
   // 10.10.25.2 - - [21/May/2012:16:29:41 +0200] "GET /unirel_logo.gif HTTP/1.1" 200 3414 "http://www.unirel.com/" "Mozilla/5.0 (X11; Linux x86_64)"
   // ------------------------------------------------------------------------------------------------------------------------------------------------- 

#ifdef U_LOG_ENABLE
   static char iov_buffer[20];
   static struct iovec iov_vec[10];
# if !defined(U_CACHE_REQUEST_DISABLE) || defined(U_SERVER_CHECK_TIME_BETWEEN_REQUEST) 
   static uint32_t request_offset, referer_offset, agent_offset;
# endif

   static void    initApacheLikeLog();
   static void prepareApacheLikeLog();
   static void   resetApacheLikeLog()
      {
      U_TRACE_NO_PARAM(0, "UHTTP::resetApacheLikeLog()")

      iov_vec[6].iov_len  =
      iov_vec[8].iov_len  = 1;
      iov_vec[6].iov_base =
      iov_vec[8].iov_base = (caddr_t) "-";
      }
#endif

   // CSP (C Servlet Page)

   typedef int (*iPFipvc)(int,const char**);

   class UCServletPage {
   public:

   // Check for memory error
   U_MEMORY_TEST

   // Allocator e Deallocator
   U_MEMORY_ALLOCATOR
   U_MEMORY_DEALLOCATOR

   int size;
   void* relocated;
   iPFipvc prog_main;

   // COSTRUTTORI

   UCServletPage()
      {
      U_TRACE_REGISTER_OBJECT(0, UCServletPage, "", 0)

      size      = 0;
      relocated = 0;
      prog_main = 0;
      }

   ~UCServletPage()
      {
      U_TRACE_UNREGISTER_OBJECT(0, UCServletPage)

      if (relocated) UMemoryPool::_free(relocated, size, 1);
      }

   bool compile(const UString& program);

   // DEBUG

#if defined(U_STDCPP_ENABLE) && defined(DEBUG)
   const char* dump(bool reset) const U_EXPORT;
#endif

   private:
#ifdef U_COMPILER_DELETE_MEMBERS
   UCServletPage(const UCServletPage&) = delete;
   UCServletPage& operator=(const UCServletPage&) = delete;
#else
   UCServletPage(const UCServletPage&)            {}
   UCServletPage& operator=(const UCServletPage&) { return *this; }
#endif      
   };

   typedef void (*vPFstr)(UString&);

#ifdef USE_PAGE_SPEED // (Google Page Speed)
   typedef void (*vPFpcstr)(const char*, UString&);

   class UPageSpeed : public UDynamic {
   public:

   vPFpcstr minify_html;
   vPFstr optimize_gif, optimize_png, optimize_jpg;

   // COSTRUTTORI

   UPageSpeed()
      {
      U_TRACE_REGISTER_OBJECT(0, UPageSpeed, "", 0)

      minify_html  = 0;
      optimize_gif = optimize_png = optimize_jpg = 0;
      }

   ~UPageSpeed()
      {
      U_TRACE_UNREGISTER_OBJECT(0, UPageSpeed)
      }

#if defined(U_STDCPP_ENABLE) && defined(DEBUG)
   const char* dump(bool reset) const U_EXPORT;
#endif

   private:
#ifdef U_COMPILER_DELETE_MEMBERS
   UPageSpeed(const UPageSpeed&) = delete;
   UPageSpeed& operator=(const UPageSpeed&) = delete;
#else
   UPageSpeed(const UPageSpeed&) : UDynamic() {}
   UPageSpeed& operator=(const UPageSpeed&)   { return *this; }
#endif      
   };

   static UPageSpeed* page_speed;
#endif

#ifdef USE_LIBV8 // (Google V8 JavaScript Engine)
   class UV8JavaScript : public UDynamic {
   public:

   vPFstr runv8;

   // COSTRUTTORI

   UV8JavaScript()
      {
      U_TRACE_REGISTER_OBJECT(0, UV8JavaScript, "", 0)

      runv8 = 0;
      }

   ~UV8JavaScript()
      {
      U_TRACE_UNREGISTER_OBJECT(0, UV8JavaScript)
      }

#if defined(U_STDCPP_ENABLE) && defined(DEBUG)
   const char* dump(bool reset) const U_EXPORT;
#endif

   private:
#ifdef U_COMPILER_DELETE_MEMBERS
   UV8JavaScript(const UV8JavaScript&) = delete;
   UV8JavaScript& operator=(const UV8JavaScript&) = delete;
#else
   UV8JavaScript(const UV8JavaScript&) : UDynamic() {}
   UV8JavaScript& operator=(const UV8JavaScript&)   { return *this; }
#endif      
   };

   static UV8JavaScript* v8_javascript;
#endif

#ifdef USE_PHP // (wrapper to embed the PHP interpreter)
   class UPHP : public UDynamic {
   public:

   vPF php_end;
   bPFpc runPHP;

   // COSTRUTTORI

   UPHP()
      {
      U_TRACE_REGISTER_OBJECT(0, UPHP, "", 0)

      runPHP  = 0;
      php_end = 0;
      }

   ~UPHP()
      {
      U_TRACE_UNREGISTER_OBJECT(0, UPHP)
      }

#if defined(U_STDCPP_ENABLE) && defined(DEBUG)
   const char* dump(bool reset) const U_EXPORT;
#endif

   private:
#ifdef U_COMPILER_DELETE_MEMBERS
   UPHP(const UPHP&) = delete;
   UPHP& operator=(const UPHP&) = delete;
#else
   UPHP(const UPHP&) : UDynamic() {}
   UPHP& operator=(const UPHP&)   { return *this; }
#endif      
   };

   static UPHP* php_embed;
#endif

#ifdef USE_RUBY // (wrapper to embed the RUBY interpreter)
   class URUBY : public UDynamic {
   public:

   vPF ruby_end;
   bPFpcpc runRUBY;

   // COSTRUTTORI

   URUBY()
      {
      U_TRACE_REGISTER_OBJECT(0, URUBY, "", 0)

      runRUBY  = 0;
      ruby_end = 0;
      }

   ~URUBY()
      {
      U_TRACE_UNREGISTER_OBJECT(0, URUBY)
      }

#if defined(U_STDCPP_ENABLE) && defined(DEBUG)
   const char* dump(bool reset) const U_EXPORT;
#endif

   private:
#ifdef U_COMPILER_DELETE_MEMBERS
   URUBY(const URUBY&) = delete;
   URUBY& operator=(const URUBY&) = delete;
#else
   URUBY(const URUBY&) : UDynamic() {}
   URUBY& operator=(const URUBY&)   { return *this; }
#endif      
   };

   static URUBY* ruby_embed;
   static bool ruby_on_rails;
#endif

#if defined(U_ALIAS) && defined(USE_LIBPCRE) // REWRITE RULE
   class RewriteRule {
   public:

   // Check for memory error
   U_MEMORY_TEST

   // Allocator e Deallocator
   U_MEMORY_ALLOCATOR
   U_MEMORY_DEALLOCATOR

   // COSTRUTTORI

   UPCRE key;
   UString replacement;

   RewriteRule(const UString& _key, const UString& _replacement) : key(_key, PCRE_FOR_REPLACE), replacement(_replacement)
      {
      U_TRACE_REGISTER_OBJECT(0, RewriteRule, "%V,%V", _key.rep, _replacement.rep)

      key.study();
      }

   ~RewriteRule()
      {
      U_TRACE_UNREGISTER_OBJECT(0, RewriteRule)
      }

#if defined(U_STDCPP_ENABLE) && defined(DEBUG)
   const char* dump(bool reset) const U_EXPORT;
#endif

   private:
#ifdef U_COMPILER_DELETE_MEMBERS
   RewriteRule& operator=(const RewriteRule&) = delete;
#else
   RewriteRule& operator=(const RewriteRule&) { return *this; }
#endif      
   };

   static UVector<RewriteRule*>* vRewriteRule;
#endif      

   // DOCUMENT ROOT CACHE

   class UFileCacheData {
   public:

   // Check for memory error
   U_MEMORY_TEST

   // Allocator e Deallocator
   U_MEMORY_ALLOCATOR
   U_MEMORY_DEALLOCATOR

   void* ptr;               // data
   UVector<UString>* array; // content, header, gzip(content, header)
   time_t mtime;            // time of last modification
   time_t expire;           // expire time of the entry
   uint32_t size;           // size content
   int wd;                  // if directory it is a "watch list" associated with an inotify instance...
   mode_t mode;             // file type
   int mime_index;          // index file mime type
   int fd;                  // file descriptor
   bool link;               // true => ptr point to another entry

   // COSTRUTTORI

    UFileCacheData();
    UFileCacheData(const UFileCacheData& elem);
   ~UFileCacheData();

   // STREAM

#ifdef U_STDCPP_ENABLE
   friend U_EXPORT istream& operator>>(istream& is,       UFileCacheData& d);
   friend U_EXPORT ostream& operator<<(ostream& os, const UFileCacheData& d);

#  ifdef DEBUG
   const char* dump(bool reset) const U_EXPORT;
#  endif
#endif

   private:
#ifdef U_COMPILER_DELETE_MEMBERS
   UFileCacheData& operator=(const UFileCacheData&) = delete;
#else
   UFileCacheData& operator=(const UFileCacheData&) { return *this; }
#endif

   template <class T> friend void u_construct(const T**,bool);
   };

   static UString* cache_file_mask;
   static UString* cache_avoid_mask;
   static UString* cache_file_store;
   static UString* nocache_file_mask;
   static UFileCacheData* file_data;
   static UHashMap<UFileCacheData*>* cache_file;
   static UFileCacheData* file_not_in_cache_data;

   static bool isDataFromCache()
      {
      U_TRACE_NO_PARAM(0, "UHTTP::isDataFromCache()")

      U_INTERNAL_ASSERT_POINTER(file_data)

      U_INTERNAL_DUMP("file_data->array = %p", file_data->array)

      if (file_data->array != 0) U_RETURN(true);

      U_RETURN(false);
      }

   static bool isDataCompressFromCache()
      {
      U_TRACE_NO_PARAM(0, "UHTTP::isDataCompressFromCache()")

      U_INTERNAL_ASSERT_POINTER(file_data)
      U_INTERNAL_ASSERT_POINTER(file_data->array)

      if (file_data->array->size() > 2) U_RETURN(true);

      U_RETURN(false);
      }

   static void checkFileForCache()
      {
      U_TRACE_NO_PARAM(0, "UHTTP::checkFileForCache()")

      U_INTERNAL_ASSERT_POINTER(pathname)

      file->setPath(*pathname);

      if (file->stat()) manageDataForCache(); // NB: file->stat() get also the size of the file...
      }

   static void renewFileDataInCache();
   static void checkFileInCache(const char* path, uint32_t len);

   static bool isFileInCache()
      {
      U_TRACE_NO_PARAM(0, "UHTTP::isFileInCache()")

      checkFileInCache(U_FILE_TO_PARAM(*file));

      if (file_data) U_RETURN(true);

      U_RETURN(false);
      }

   static uint32_t old_path_len;

   static void checkFileInCache1(const char* path, uint32_t len)
      {
      U_TRACE(0, "UHTTP::checkFileInCache1(%.*S,%u)", len, path, len)

      U_INTERNAL_DUMP("old_path_len = %u", old_path_len)

      if (old_path_len != len) checkFileInCache(path, (old_path_len = len));
      }

   static UString getDataFromCache(int idx);

   static UString   getBodyFromCache() { return getDataFromCache(0); }
   static UString getHeaderFromCache() { return getDataFromCache(1); };

   static UString   getBodyCompressFromCache() { return getDataFromCache(2); }
   static UString getHeaderCompressFromCache() { return getDataFromCache(3); };

   static UFileCacheData* getFileInCache(const char* path, uint32_t len);

private:
   static void    handlerResponse();
   static UString getHTMLDirectoryList() U_NO_EXPORT;

   static void setMimeIndex() // NB: it is used by server_plugin_ssi...
      {
      U_TRACE_NO_PARAM(0, "UHTTP::setMimeIndex()")

      U_INTERNAL_ASSERT_POINTER(file)
      U_ASSERT_EQUALS(UClientImage_Base::isRequestNotFound(), false)

      mime_index = U_unknow;

      const char* ptr = u_getsuffix(U_FILE_TO_PARAM(*file));

      if (ptr) (void) u_get_mimetype(ptr+1, &mime_index);
      }

#ifdef DEBUG
   static bool cache_file_check_memory();
   static bool check_memory(UStringRep* key, void* value) U_NO_EXPORT;
#endif

#if defined(U_ALIAS) && defined(USE_LIBPCRE) // REWRITE RULE
   static void processRewriteRule() U_NO_EXPORT;
#endif

#if defined(HAVE_SYS_INOTIFY_H) && defined(U_HTTP_INOTIFY_SUPPORT) && !defined(U_SERVER_CAPTIVE_PORTAL)
   static int               inotify_wd;
   static char*             inotify_name;
   static uint32_t          inotify_len;
   static UString*          inotify_pathname;
   static UStringRep*       inotify_dir;
   static UFileCacheData*   inotify_file_data;

   static void in_READ();
   static void initInotify();
   static void setInotifyPathname() U_NO_EXPORT;
   static bool getInotifyPathDirectory(UStringRep* key, void* value) U_NO_EXPORT;
   static bool checkForInotifyDirectory(UStringRep* key, void* value) U_NO_EXPORT;
#endif

#ifdef U_STATIC_ONLY
   static uint32_t getHtmlEncodedForResponse(char* buffer, uint32_t size, const char* fmt);
   static void     loadStaticLinkedServlet(const char* name, uint32_t len, vPFi runDynamicPage) U_NO_EXPORT;
#endif      

   static void checkPath() U_NO_EXPORT;
   static bool callService() U_NO_EXPORT;
   static bool runDynamicPage() U_NO_EXPORT;
   static bool readBodyRequest() U_NO_EXPORT;
   static bool processFileCache() U_NO_EXPORT;
   static bool readHeaderRequest() U_NO_EXPORT;
   static void processGetRequest() U_NO_EXPORT;
   static void manageDataForCache() U_NO_EXPORT;
   static bool processAuthorization() U_NO_EXPORT;
   static bool checkPath(uint32_t len) U_NO_EXPORT;
   static void checkRequestForHeader() U_NO_EXPORT;
   static bool checkGetRequestIfRange() U_NO_EXPORT;
   static bool checkGetRequestIfModified() U_NO_EXPORT;
   static void setCGIShellScript(UString& command) U_NO_EXPORT;
   static bool checkIfSourceHasChangedAndCompileUSP() U_NO_EXPORT;
   static void removeDataSession(const UString& token) U_NO_EXPORT;
   static bool checkIfUSP(UStringRep* key, void* value) U_NO_EXPORT;
   static bool compileUSP(const char* path, uint32_t len) U_NO_EXPORT;
   static bool checkIfUSPLink(UStringRep* key, void* value) U_NO_EXPORT;
   static int  checkGetRequestForRange(const UString& data) U_NO_EXPORT;
   static int  sortRange(const void* a, const void* b) __pure U_NO_EXPORT;
   static bool addHTTPVariables(UStringRep* key, void* value) U_NO_EXPORT;
   static bool splitCGIOutput(const char*& ptr1, const char* ptr2) U_NO_EXPORT;
   static void putDataInCache(const UString& fmt, UString& content) U_NO_EXPORT;
   static bool checkDataSession(const UString& token, time_t expire) U_NO_EXPORT;
   static bool readDataChunked(USocket* sk, UString* pbuffer, UString& body) U_NO_EXPORT;
   static void setResponseForRange(uint32_t start, uint32_t end, uint32_t header) U_NO_EXPORT;

#ifdef U_COMPILER_DELETE_MEMBERS
   UHTTP(const UHTTP&) = delete;
   UHTTP& operator=(const UHTTP&) = delete;
#else
   UHTTP(const UHTTP&)            {}
   UHTTP& operator=(const UHTTP&) { return *this; }
#endif      

   friend class USSIPlugIn;
   friend class UHttpPlugIn;
};

template <> inline void u_destroy(const UHTTP::UFileCacheData* elem)
{
   U_TRACE(0, "u_destroy<UFileCacheData>(%p)", elem)

   if (elem <= (const void*)0x0000ffff) U_ERROR("u_destroy<UFileCacheData>(%p)", elem);

   // NB: we need this check if we are at the end of UHashMap<UHTTP::UFileCacheData*>::operator>>()...

   if (UHashMap<void*>::istream_loading)
      {
      ((UHTTP::UFileCacheData*)elem)->ptr   =
      ((UHTTP::UFileCacheData*)elem)->array = 0;
      }

   delete elem;
}
#endif
