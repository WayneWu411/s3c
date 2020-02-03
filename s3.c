#include <curl/curl.h>
#include "s3.h"

#define S3_ENDPOINT "s3-ap-southeast-2"
#define S3_KEY      "YOUR_S3_KEY"
#define S3_SECRET   "YOUR_S3_SECRET"

static int getCurrentDate(char *dst, size_t dstLen)
{
    int ret = 0;
    if(0 != (ret = executeAndReturn("date -R | tr -d '\\n'", dst, sizeof(dst)-1))) {
        error("fail to execute date -R, ret = %d", ret);
        return ret;
    }

    debug("dst = %s", dst);

    return 0;
}

/* 
 * PUT\n\n${contentType}\n${dateValue}\n/${bucket}/${path}/${file}
 */
#define STRING_TO_SIGN_FMT  "PUT\\n\\n%s\\n%s\\n/%s/%s/%s"
static void getStringToSign(char *dst, size_t dstLen, const char *contentType, const char *date, const char *bucket, const char *s3Path, const char *s3FileName)
{

    strcpy(dst, "PUT\n");
    strcat(dst, "\n");
    strcat(dst, contentType);
    strcat(dst, "\n");
    strcat(dst, date);
    strcat(dst, "\n");
    strcat(dst, "/");
    strcat(dst, bucket);
    strcat(dst, "/");
    strcat(dst, s3Path);
    strcat(dst, "/");
    strcat(dst, s3FileName);
}

/*
 * echo -en ${stringToSign} | openssl sha1 -hmac ${s3Secret} -binary | base64
 * Return Value:
 * -1: Error
 * >=0: Success and the Length of Signature
 */
static int getSignature(char *dst, size_t dstLen, const char *stringToSign)
{
    int ret = 0;

    /* openssl sha1 -hmac ${s3Secret} -binary */
    char hmacBuf[256] = {0};
    unsigned int hmacBufLen = sizeof(hmacBuf);
    if(NULL == HMAC(EVP_sha1(), S3_SECRET, strlen(S3_SECRET), stringToSign, strlen(stringToSign), hmacBuf, &hmacBufLen)) {
        error("HMAC() failed");
        ERR_clear_error();
        return -1;
    }

    /* base64 */
    char base64Buf[2048] = {0};
    int base64BufLen = sizeof(base64Buf);
    base64BufLen = base64Encode(base64Buf, hmacBuf, hmacBufLen);    /* base64Encode will append a zero termination at the end */
    debug("base64Buf(%d): %s", base64BufLen, base64Buf);
    if(dstLen < base64BufLen) {
        error("destination buffer too small, dstLen = %d, base64BufLen = %d", dstLen, base64BufLen);
        return -1;
    }

    strncpy(dst, base64Buf, dstLen-1);

    return base64BufLen;
}

static void getHttpHeaderStringHost(char *dst, size_t dstLen, const char *endPoint, const char *bucket)
{
    /* Host: ${bucket}.s3-ap-southeast-2.amazonaws.com */
    snprintf(dst, dstLen, "Host: %s.%s.amazonaws.com", bucket, S3_ENDPOINT);
}

static void getHttpHeaderStringDate(char *dst, size_t dstLen)
{
    char date[64] = {0};
    if(0 != (getCurrentDate(date, sizeof(date)))) {
        return;
    }

    /* Date: ${dateValue} */
    snprintf(dst, dstLen, "Date: %s", date);
}

static void getHttpHeaderStringContentType(char *dst, size_t dstLen, const char *contentType)
{
    /* Content-Type: ${contentType} */
    snprintf(dst, dstLen, "Content-Type: %s", contentType);
}

static void getHttpHeaderStringAuthorization(char *dst, size_t dstLen, const char *contentType, const char *bucket, const char *s3Path, const char *s3FileName)
{
    char date[64] = {0};
    if(0 != getCurrentDate(date, sizeof(date))) {
        return;
    }
    
    char *stringToSign[2048] = {0};
    getStringToSign(stringToSign, sizeof(stringToSign), contentType, date, bucket, s3Path, s3FileName);

    char signature[2048] = {0};
    int signatureLen = 0;
    signatureLen = getSignature(signature, sizeof(signature), stringToSign);

    /* Authorization: AWS ${s3Key}:${signature} */
    snprintf(dst, dstLen, "Authorization: AWS %s:%s", S3_KEY, signature);
    debug("Authorization Header = %s", dst);
}

static void getS3UploadURL(char *dst, size_t dstLen, const char *bucket, const char *s3Path, const char *s3FileName)
{
    /* https://${bucket}.s3-ap-southeast-2.amazonaws.com/vmms/${file} */
    snprintf(dst, dstLen, "https://%s.%s.amazonaws.com/%s/%s", bucket, S3_ENDPOINT, s3Path, s3FileName);
}

int s3UploadFile(const char *localFile, const char *contentType, const char *bucket, const char *s3Path, const char *s3FileName)
{
    FILE *fpSrc = NULL;
    struct stat fileInfo = {0};

    fpSrc = fopen(localFile, "rb");
    if(NULL == fpSrc) {
        error("fail to open %s", localFile);
        return -1;
    }

    stat(localFile, &fileInfo);

    curl_global_init(CURL_GLOBAL_ALL);

    CURL *curl = curl_easy_init();
    if(NULL == curl) {
        error("curl_easy_init failed");
        curl_global_cleanup();
        fclose(fpSrc);
        return -2;
    }    
    
    /* HTTP Headers */
    struct curl_slist *headers = NULL;
        
    char headerHost[64] = {0};
    getHttpHeaderStringHost(headerHost, sizeof(headerHost), S3_ENDPOINT, bucket);
    headers = curl_slist_append(headers, headerHost);

    char headerDate[64] = {0};
    getHttpHeaderStringDate(headerDate, sizeof(headerDate));
    headers = curl_slist_append(headers, headerDate);

    char headerContentType[64] = {0};
    getHttpHeaderStringContentType(headerContentType, sizeof(headerContentType), contentType);
    headers = curl_slist_append(headers, headerContentType);

    char headerAuthorization[1024] = {0};
    getHttpHeaderStringAuthorization(headerAuthorization, sizeof(headerAuthorization), contentType, bucket, s3Path, s3FileName);
    headers = curl_slist_append(headers, headerAuthorization);

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
    //curl_easy_setopt(curl, CURLOPT_READFUNCTION, myRead);

    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1);

    curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_ANY);

    curl_easy_setopt(curl, CURLOPT_READDATA, fpSrc);

    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)fileInfo.st_size);

    curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
    curl_easy_setopt(curl, CURLOPT_CAINFO, "cacert.pem");
    curl_easy_setopt(curl, CURLOPT_CAPATH, "/vol/program");

    char s3URL[256] = {0};
    getS3UploadURL(s3URL, sizeof(s3URL), bucket, s3Path, s3FileName);
    curl_easy_setopt(curl, CURLOPT_URL, s3URL);

    CURLcode ret = curl_easy_perform(curl);
    if(CURLE_OK != ret) {
        error("curl_easy_perform() failed, %s", curl_easy_strerror(ret));
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    curl_global_cleanup();

    fclose(fpSrc);

    return ret;
}