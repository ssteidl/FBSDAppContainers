#include "url_cmd.h"
#include "tcl_util.h"

#include "tcl.h"
#include "curl/urlapi.h"
#include <string>
#include <iostream> //TODO
namespace
{

std::string url_curl_strerr(CURLUcode error_code)
{
    switch(error_code)
    {
    case CURLUE_OK:
        return std::string("OK");
    case CURLUE_BAD_HANDLE:
        return std::string("BAD_HANDLE");/* 1 */
    case CURLUE_BAD_PARTPOINTER:
        return std::string("BAD_PART_POINTER");/* 2 */
    case CURLUE_MALFORMED_INPUT:
        return std::string("Malformed INPUT"); /* 3 */
    case CURLUE_BAD_PORT_NUMBER:     /* 4 */
        return std::string("Bad port number");
    case CURLUE_UNSUPPORTED_SCHEME:
        return std::string("Unsupported scheme");/* 5 */
    case CURLUE_URLDECODE:
        return std::string("Decode error");/* 6 */
    case CURLUE_OUT_OF_MEMORY:
        return std::string("Out of memory");/* 7 */
    case CURLUE_USER_NOT_ALLOWED:
        return std::string("User not allowed");/* 8 */
    case CURLUE_UNKNOWN_PART:
        return std::string("Unknown part");/* 9 */
    case CURLUE_NO_SCHEME:
        return std::string("No scheme");/* 10 */
    case CURLUE_NO_USER:
        return std::string("No user");/* 11 */
    case CURLUE_NO_PASSWORD:
        return std::string("No password");/* 12 */
    case CURLUE_NO_OPTIONS:          /* 13 */
        return std::string("No options");
    case CURLUE_NO_HOST:
        return std::string("No host");/* 14 */
    case CURLUE_NO_PORT:
        return std::string("No port");/* 15 */
    case CURLUE_NO_QUERY:
        return std::string("No query");/* 16 */
    case CURLUE_NO_FRAGMENT:
        return std::string("No fragment");
    default:
        return std::string("Unknown error");
    }
}

class url_result
{
    CURLUcode m_error_code;
    std::string m_msg_prefix;
public:
    url_result(CURLUcode error_code,
              const std::string& msg_prefix)
        : m_error_code(error_code),
          m_msg_prefix(msg_prefix)
    {}

    url_result()
        : m_error_code(CURLUE_OK),
          m_msg_prefix("UNKNOWN")
    {}

    std::string msg()
    {
        return m_msg_prefix + std::string(": ") + url_curl_strerr(m_error_code);
    }

    CURLUcode error_code()
    {
        return m_error_code;
    }
};

class url
{
    CURLU *h;
    char* m_scheme;
    char* m_user;
    char* m_password;
    char* m_host;
    char* m_port;
    char* m_path;
    char* m_fragment;
    char* m_query;

    url()
        : h(curl_url()),
          m_scheme(nullptr),
          m_user(nullptr),
          m_password(nullptr),
          m_host(nullptr),
          m_port(nullptr),
          m_path(nullptr),
          m_fragment(nullptr),
          m_query(nullptr)
    {}

    void free_part(char* part)
    {
        if(part)
        {
            curl_free(part);
        }
    }

    static std::string attribute_str(char* attribute)
    {
        if(attribute)
        {
            return std::string(attribute);
        }
        else
        {
            return std::string();
        }
    }
public:

    static std::unique_ptr<url> parse(const std::string& _url, url_result& result)
    {
        CURLUcode uc;

        std::unique_ptr<url> url_ptr(new url());
        url& u = *url_ptr;
        /* parse a full URL */
        uc = curl_url_set(u.h, CURLUPART_URL, _url.c_str(), CURLU_NON_SUPPORT_SCHEME);
        if(uc)
        {
          result = url_result(uc, "Setting URL");
          return nullptr;
        }

        /* extract scheme name from the parsed URL */
        uc = curl_url_get(u.h, CURLUPART_SCHEME, &u.m_scheme, 0);
        if(uc) {
          result = url_result(uc, "Parse scheme failed");
          return nullptr;
        }

        (void)curl_url_get(u.h, CURLUPART_USER, &u.m_user, 0);
        (void)curl_url_get(u.h, CURLUPART_PASSWORD, &u.m_password, 0);

        (void)curl_url_get(u.h, CURLUPART_HOST, &u.m_host, 0);

        (void)curl_url_get(u.h, CURLUPART_PORT, &u.m_port, 0);

        /* extract the path from the parsed URL */
        uc = curl_url_get(u.h, CURLUPART_PATH, &u.m_path, 0);
        if(uc) {
          result = url_result(uc, "Parse path failed");
          return nullptr;
        }

        (void)curl_url_get(u.h, CURLUPART_FRAGMENT, &u.m_fragment, 0);
        (void)curl_url_get(u.h, CURLUPART_QUERY, &u.m_query, 0);

        result = url_result(CURLUE_OK, "Parse success");
        return url_ptr;
    }

    /*TODO: make accessor methods for url parts.  These methods should return strings.*/
    std::string scheme() const
    {
        return attribute_str(m_scheme);
    }

    std::string user() const
    {
        return attribute_str(m_user);
    }

    std::string password() const
    {
        return attribute_str(m_password);
    }

    std::string host() const
    {
        return attribute_str(m_host);
    }

    std::string port() const
    {
        return attribute_str(m_port);
    }

    std::string path() const
    {
        return attribute_str(m_path);
    }

    std::string fragment() const
    {
        return attribute_str(m_fragment);
    }

    std::string query() const
    {
        return attribute_str(m_query);
    }

    ~url()
    {
        free_part(m_scheme);
        free_part(m_user);
        free_part(m_password);
        free_part(m_host);
        free_part(m_port);
        free_part(m_path);
        free_part(m_fragment);
        free_part(m_query);
        curl_url_cleanup(h);
    }
};
}

int Vessel_ParseURL(void *clientData, Tcl_Interp *interp,
                  int objc, struct Tcl_Obj *const *objv)
{
    if(objc != 2)
    {
        Tcl_WrongNumArgs(interp, objc, objv, "url");
        return TCL_ERROR;
    }

    char *url_string = Tcl_GetStringFromObj(objv[1], nullptr);
    std::cerr << "Url string: " << url_string << std::endl;
    std::unique_ptr<url> u;
    url_result result;
    u = url::parse(url_string, result);

    if(!u)
    {
        std::string msg = result.msg();
        Tcl_Obj* result_str = Tcl_NewStringObj(msg.c_str(), msg.size());
        Tcl_SetObjResult(interp, result_str);
        return TCL_ERROR;
    }

    vessel::tclobj_ptr url_dict = vessel::create_tclobj_ptr(Tcl_NewDictObj());
    int tcl_err = Tcl_DictObjPut(interp, url_dict.get(),
                                 Tcl_NewStringObj("scheme", -1),
                                 Tcl_NewStringObj(u->scheme().c_str(), -1));
    if(tcl_err) return tcl_err;

    tcl_err = Tcl_DictObjPut(interp, url_dict.get(),
                             Tcl_NewStringObj("user", -1),
                             Tcl_NewStringObj(u->user().c_str(), -1));
    if(tcl_err) return tcl_err;

    tcl_err = Tcl_DictObjPut(interp, url_dict.get(),
                             Tcl_NewStringObj("password", -1),
                             Tcl_NewStringObj(u->password().c_str(), -1));
    if(tcl_err) return tcl_err;

    tcl_err = Tcl_DictObjPut(interp, url_dict.get(),
                             Tcl_NewStringObj("host", -1),
                             Tcl_NewStringObj(u->host().c_str(), -1));
    if(tcl_err) return tcl_err;

    tcl_err = Tcl_DictObjPut(interp, url_dict.get(),
                             Tcl_NewStringObj("port", -1),
                             Tcl_NewStringObj(u->port().c_str(), -1));
    if(tcl_err) return tcl_err;

    tcl_err = Tcl_DictObjPut(interp, url_dict.get(),
                             Tcl_NewStringObj("path", -1),
                             Tcl_NewStringObj(u->path().c_str(), -1));
    if(tcl_err) return tcl_err;

    tcl_err = Tcl_DictObjPut(interp, url_dict.get(),
                             Tcl_NewStringObj("fragment", -1),
                             Tcl_NewStringObj(u->fragment().c_str(), -1));
    if(tcl_err) return tcl_err;

    tcl_err = Tcl_DictObjPut(interp, url_dict.get(),
                             Tcl_NewStringObj("query", -1),
                             Tcl_NewStringObj(u->query().c_str(), -1));
    if(tcl_err) return tcl_err;

    Tcl_SetObjResult(interp, url_dict.release());
    return TCL_OK;
}
