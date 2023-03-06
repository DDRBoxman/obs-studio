#include <stdlib.h>
#include <string.h>

#include <util/curl/curl-helper.h>
#include "util/base.h"

struct whip_memory {
  char *response;
  size_t size;
};
 
static size_t cb(void *data, size_t size, size_t nmemb, void *clientp)
{
  size_t realsize = size * nmemb;
  struct whip_memory *mem = (struct whip_memory *)clientp;
 
  char *ptr = realloc(mem->response, mem->size + realsize + 1);
  if(ptr == NULL)
    return 0;  /* out of memory! */
 
  mem->response = ptr;
  memcpy(&(mem->response[mem->size]), data, realsize);
  mem->size += realsize;
  mem->response[mem->size] = 0;
 
  return realsize;
}

char* whip_it(const char *url, const char *sdp) {
	CURL *curl_handle;
	CURLcode res;
	long response_code;

	curl_handle = curl_easy_init();

    struct whip_memory chunk = {0};

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/sdp");

    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, cb);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    curl_easy_setopt(curl_handle, CURLOPT_POST, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, sdp);
    

    res = curl_easy_perform(curl_handle);
    if (res != CURLE_OK) {
		blog(LOG_WARNING,
		     "failed to make whip request: %s",
		     curl_easy_strerror(res));
		curl_easy_cleanup(curl_handle);
        free(chunk.response);
        curl_slist_free_all(headers);
        return NULL;
	} 

	curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &response_code);
	if (response_code != 200) {
		blog(LOG_WARNING,
		     "whip request returned code: %ld",
		     response_code);
        free(chunk.response);
		curl_easy_cleanup(curl_handle);
		curl_slist_free_all(headers);
		return NULL;
	}

    curl_slist_free_all(headers);
	curl_easy_cleanup(curl_handle);

    if (chunk.size == 0) {
		blog(LOG_WARNING,
		     "whip request returned empty response");
		free(chunk.response);
		return NULL;
	}

    return chunk.response;
}
