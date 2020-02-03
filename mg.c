
#define MG_API_KEY  "api:MGKEY"
#define MG_API_URL  "https://api.mailgun.net/v3/MYDOMAIN/messages"
int mgSendMail(const char *from, const char *to, const char *subject, const char *text)
{
    curl_global_init(CURL_GLOBAL_ALL);

    struct curl_httppost *post = NULL;
    struct curl_httppost *last = NULL;

    /* POST FORM DATA */
    curl_formadd(&post, &last,
        CURLFORM_COPYNAME, "from",
        CURLFORM_COPYCONTENTS, from,
        CURLFORM_END);
    curl_formadd(&post, &last,
        CURLFORM_COPYNAME, "to",
        CURLFORM_COPYCONTENTS, to,
        CURLFORM_END);
    curl_formadd(&post, &last,
        CURLFORM_COPYNAME, "subject",
        CURLFORM_COPYCONTENTS, subject,
        CURLFORM_END);
    curl_formadd(&post, &last,
        CURLFORM_COPYNAME, "text",
        CURLFORM_COPYCONTENTS, text,
        CURLFORM_END);
    curl_formadd(&post, &last,
        CURLFORM_COPYNAME, "o:tracking",
        CURLFORM_COPYCONTENTS, "false",
        CURLFORM_END);

    CURL *curl = curl_easy_init();
    if(NULL == curl) {
        error("curl_easy_init failed");
        curl_global_cleanup();
        return -1;
    }

    curl_easy_setopt(curl, CURLOPT_URL, MG_API_URL);
    #if 0
    curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
    curl_easy_setopt(curl, CURLOPT_CAINFO, "/vol/program/cacert.pem");
    #else
    curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
    #endif
    curl_easy_setopt(curl, CURLOPT_USERPWD, MG_API_KEY);

    curl_easy_setopt(curl, CURLOPT_HTTPPOST, post);

    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);

    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);

    int ret = 0;
    CURLcode curlRet = curl_easy_perform(curl);
    if(CURLE_OK != curlRet) {
        error("curl_easy_perform() failed, %s", curl_easy_strerror(curlRet));
        ret = -2;
    }

    curl_easy_cleanup(curl);
    curl_formfree(post);
    curl_global_cleanup();

    return ret;
}