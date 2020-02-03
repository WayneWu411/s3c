
typedef struct HTTP_BUF_T
{
    char *buf;
    size_t currentLen;
    size_t maxLen;
} HTTP_BUF;

static int httpGetXferCallback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
    static long int secondsSincePowerup = 0;

    if((0 == secondsSincePowerup) ||
        (getSecondsSincePowerUp() - secondsSincePowerup) >= 1) {  /* feedDog every second */
        feedDog();
        
        secondsSincePowerup = getSecondsSincePowerUp();
    }

    return 0;
}

static size_t httpWriteFunc(void *content, size_t size, size_t nmemb, void *userp)
{
    size_t contentSize = size * nmemb;
    HTTP_BUF *httpWriteBuf = (HTTP_BUF *)userp;
    
    size_t remainSize = httpWriteBuf->maxLen - httpWriteBuf->currentLen;
    if(contentSize > (remainSize - 1)) {
        error("HTTP Get Buffer too small");
        return 0;
    }

    memmove(httpWriteBuf->buf + httpWriteBuf->currentLen, content, contentSize);
    httpWriteBuf->currentLen += contentSize;

    return contentSize;
}

int curlHttpGet(const char *url, char *recvBuf, int maxLen)
{
    if(!isCurlInited()) {
        error("curl not initilized!");
        return -1;
    }

    CURL *curl = curl_easy_init();
    if(NULL == curl) {
        error("curl_easy_init failed");
        return -2;
    }

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "User-Agent: CIRCUMVEND 1.0");
    headers = curl_slist_append(headers, "Accept: text/html");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    curl_easy_setopt(curl, CURLOPT_URL, url);

    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

    HTTP_BUF httpWriteBuf = {0};
    httpWriteBuf.buf = recvBuf;
    httpWriteBuf.currentLen = 0;
    httpWriteBuf.maxLen= maxLen;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, httpWriteFunc);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&httpWriteBuf);

    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, httpGetXferCallback);

    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);

    CURLcode curlRet = curl_easy_perform(curl);
    if(CURLE_OK != curlRet) {
        error("curl_easy_perform() failed, %s", curl_easy_strerror(curlRet));

        isBackupIpInUse = !isBackupIpInUse;

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        return -3;
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return strlen(httpWriteBuf.buf);
}

int curlHttpPostFile(const char *url, const char *fileName, char *recvBuf, int maxLen)
{
    if(!isCurlInited()) {
        error("curl not initialized!");
        return -1;
    }

    FILE *fpSrc = fopen(fileName, "rb");
    if(NULL == fpSrc) {
        error("fail to open %s", fileName);
        return -2;
    }

    struct stat fileInfo = {0};
    stat(fileName, &fileInfo);

    CURL *curl = curl_easy_init();
    if(NULL == curl) {
        error("curl_easy_init failed");
        fclose(fpSrc);
        return -3;
    }

    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);

    curl_easy_setopt(curl, CURLOPT_POST, 1);

    curl_easy_setopt(curl, CURLOPT_READDATA, fpSrc);
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)fileInfo.st_size);

    /* Buffer and Write Function for Response */
    HTTP_BUF httpWriteBuf = {0};
    httpWriteBuf.buf = recvBuf;
    httpWriteBuf.currentLen = 0;
    httpWriteBuf.maxLen= maxLen;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, httpWriteFunc);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&httpWriteBuf);

    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);

    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, httpPostFileXferCallback);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0);

    curl_easy_setopt(curl, CURLOPT_URL, url);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Expect:");
    headers = curl_slist_append(headers, "Content-Type: text/xml");
    headers = curl_slist_append(headers, "Accept: text/html");
    char contentLengthString[64] = {0};
    snprintf(contentLengthString, sizeof(contentLengthString), "Content-Length: %ld", fileInfo.st_size);
    headers = curl_slist_append(headers, contentLengthString);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode curlRet = curl_easy_perform(curl);
    if(CURLE_OK != curlRet) {
        error("curl_easy_perform() failed, %s", curl_easy_strerror(curlRet));
        isBackupIpInUse = !isBackupIpInUse;
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        fclose(fpSrc);
        return -4;
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    fclose(fpSrc);

    return httpWriteBuf.currentLen;
}
