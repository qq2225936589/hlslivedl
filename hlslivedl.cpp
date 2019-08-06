#include <curl/curl.h>
#include <string>
#include <iostream>
#include <unistd.h>
#include <stdio.h>
#include <conio.h>
#include <regex>
#include <iterator>
#include <io.h>
#include <fstream>
#include <getopt.h>

using namespace std;

static char m3u8[1024];
static char g_useragent[1024];
static char g_proxy[100];

static string baseurl;
static int isDEBUG = 0;

void putmsg(const char * msg) 
{
    if (!msg) return;
	FILE * m3u8h = fopen(m3u8, "ab+");
	if (m3u8h) {
		fwrite(msg, 1, strlen(msg), m3u8h);
		fwrite("\r\n", 1, 2, m3u8h);
		fclose(m3u8h);
	}
}

typedef struct targ {
    char url[1024];
    char fn[1024];
} targ;

size_t writeFunction(void *ptr, size_t size, size_t nmemb, string* data) {
    data->append((char*) ptr, size * nmemb);
    return size * nmemb;
}
size_t writeFunction2(void *ptr, size_t size, size_t nmemb, string* data) {
    data->append((char*) ptr, size * nmemb);
    return size * nmemb;
}

static void * downts(void *arg)
{
    targ *ta= (targ *)arg;
    string fullurl = ta->url;;
    string fn = ta->fn;
    
    //cout << fn << endl;
    auto curl = curl_easy_init();
    if (curl) {
    	curl_easy_setopt(curl, CURLOPT_URL, fullurl.c_str());
    	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
    	//curl_easy_setopt(curl, CURLOPT_USERPWD, "user:pass");
    	curl_easy_setopt(curl, CURLOPT_USERAGENT, g_useragent);
    	curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 50L);
    	curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
    	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 10000L);
        if(strlen(g_proxy)>0)
            curl_easy_setopt(curl, CURLOPT_PROXY, g_proxy);
    	string response_string;
    	string header_string;
    	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFunction2);
    	curl_easy_setopt(curl, CURLOPT_WRITEDATA,  & response_string);
    	curl_easy_setopt(curl, CURLOPT_HEADERDATA,  & header_string);

    	char * url;
    	long response_code;
    	double elapsed;
    	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE,  & response_code);
    	curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME,  & elapsed);
    	curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL,  & url);

    	CURLcode res = curl_easy_perform(curl);
    	if (res != CURLE_OK)
        {
    		if(isDEBUG) fprintf(stderr, "thread curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            if(isDEBUG) cout << "pthread remove " << fn << endl;
            remove(fn.c_str());
    	}
        else 
        {
    		if(isDEBUG) cout << "thread response_code " << response_code << endl;
    		if(isDEBUG) cout << "thread response_string size " << response_string.size() << endl;
    		//cout << "elapsed       " << elapsed << endl;
    		//cout << "effective url " << url << endl;
    		//cout << "response_string \n" << response_string << endl;
    		//cout << "header_string \n" << header_string << endl;
            if( response_string.size() > 0 )
            {
                //cout << fn << endl;
                FILE *tsh = fopen(fn.c_str(), "wb");
                if(tsh)
                {
                    fwrite(response_string.c_str(), 1, response_string.size(), tsh);
                    fclose(tsh);
                }
            }
    	}
    }
    curl_easy_cleanup(curl);
    curl = NULL;
    if(isDEBUG) cout << "pthread exit" << endl;
    pthread_exit(NULL);
    return NULL;    
}

string getbaseurl(string url)
{
    string text=url;
	string pattern = "((.*)/)";
	regex express(pattern);
	regex_iterator<string::const_iterator> begin(text.cbegin(), text.cend(), express);
	for (auto iter = begin; iter != sregex_iterator(); iter++)
	{
		//cout << iter->str() << endl;
        return iter->str();
    }
}

string getindex(string text)
{
    string ret = "";
	string pattern = "([0-9]{1,15})";
	regex express(pattern);
	regex_iterator<string::const_iterator> begin(text.cbegin(), text.cend(), express);
	for (auto iter = begin; iter != sregex_iterator(); iter++)
	{
        ret = iter->str();
    }
    if (ret == "" || ret.size() <= 0 ) return text;
    return ret+".ts";
}

static double duration = 0.0;
static char strduration[100];
static unsigned int dlidx = 1;
double getlen(string text)
{
    double rd = 0.0;
    string ret = "";
	string pattern = "([0-9.]{1,8})";
	regex express(pattern);
	regex_iterator<string::const_iterator> begin(text.cbegin(), text.cend(), express);
	for (auto iter = begin; iter != sregex_iterator(); iter++)
	{
        ret = iter->str();
    }
    if (ret == "" || ret.size() <= 0 ) return 0.0;
    rd = atof(ret.c_str());
    duration = duration + rd;
    
    int dhh=(unsigned int)(duration/3600);
    int dmm=(unsigned int)(duration)%3600/60;
    int dss=(unsigned int)(duration)%3600%60;
    int dms=(unsigned int)(duration*1000)%1000;
    sprintf(strduration,"%4d %5.03f %02d:%02d:%02d.%03d",
        dlidx, rd, dhh, dmm, dss, dms);
    dlidx++;
    return rd;
}

void string_replace( string &strBig, const string &strsrc, const string &strdst)
{
    string::size_type pos = 0;
    string::size_type srclen = strsrc.size();
    string::size_type dstlen = strdst.size();

    while( (pos=strBig.find(strsrc, pos)) != string::npos )
    {
        strBig.replace( pos, srclen, strdst );
        pos += dstlen;
    }
}

static unsigned int dlcount = 0;
static unsigned int firstdl = 0;

void gettsurl(string str)
{
    string fullurl;
    string text=str;
    if(dlcount==0)
    {
        string pattern = "(#EXT-X(.*)|#EXTM3U)";
        regex express(pattern);
        regex_iterator<string::const_iterator> begin(text.cbegin(), text.cend(), express);
        for (auto iter = begin; iter != sregex_iterator(); iter++)
        {
            cout << iter->str() << endl;
            putmsg(iter->str().c_str());
        }
    }
	//string pattern = "((.*).ts)";
	string pattern = "(#EXTINF:(.*)|(.*).ts)";
	regex express(pattern);
	regex_iterator<string::const_iterator> begin(text.cbegin(), text.cend(), express);
    string extinfo;

    for (auto iter = begin; iter != sregex_iterator(); iter++)
	{
        fullurl = baseurl + iter->str();
		//cout << fullurl << endl;
		//cout << iter->str() << endl;
        //downts(fullurl, iter->str());
        if( iter->str().substr(0,1) == "#" )
        {
            extinfo = iter->str();
        }
        else 
        {
            string newfn = getindex(iter->str());
            string_replace(newfn, "/", "-");
        	if (access(newfn.c_str(), 0)) {
                getlen(extinfo);
        		cout << " " << strduration << " " << newfn << endl;
        		//cout << newfn << endl;
        		putmsg(extinfo.c_str());
        		putmsg(newfn.c_str());
        		FILE * tsh = fopen(newfn.c_str(), "wb");
        		if (tsh) {
        			fclose(tsh);
        		}
        		pthread_t thread;
        		targ ta;
        		memset( & ta, 0x00, sizeof(ta));
        		strcpy(ta.url, fullurl.c_str());
        		strcpy(ta.fn, newfn.c_str());
        		pthread_create( & thread, NULL, downts,  & ta);
        	}
        }
    }
    dlcount++;
}

void help()
{
    cout << "Usage: hlslivedl [options] -i [http://...m3u8]" << endl;
    cout << "    -o set output filename" << endl;
    cout << "    -p set proxy http: https: socks4: socks4a: socks5: socks5h:" << endl;
    cout << "    -u set useragent" << endl;
    cout << "    -d debug mode" << endl;
    cout << "    Press [Q] to stop download" << endl;
    cout << "    Version 1.0.1 by NLSoft 2019.08" << endl;
}

int main(int argc, char** argv) {
    CURLcode res;
    static char urlm3u8[1024];
    memset(urlm3u8,0,1024);
    memset(g_proxy,0,100);
    memset(strduration,0,100);

    if(argc == 1)
    {
        help();
        exit(0);
    }
    strcpy(m3u8, "_index.m3u8");
    strcpy(g_useragent, "Mozilla/5.0 (iPhone; CPU iPhone OS 12_0 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/12.0 Mobile/15E148 Safari/604.1");
    
    int option_index = 0;
    char * user_name = NULL;
    while ((option_index = getopt(argc, argv, "p:o:u:i:d")) != -1) {
        switch (option_index) {
        case 'o':
            strcpy(m3u8, optarg);
            //printf("output filename is %s\n", m3u8);
            break;
        case 'u':
            strcpy(g_useragent, optarg);
            //printf("useragent is %s\n", g_useragent);
            break;
        case 'p':
            strcpy(g_proxy, optarg);
            //printf("proxy is %s\n", g_proxy);
            break;
        case 'i':
            strcpy(urlm3u8, optarg);
            //printf("m3u8 url is %s\n", urlm3u8);
            break;
        case 'd':
            isDEBUG = 1;
            //printf("select debug mode\n");
            break;
        }
    }

    if(strlen(urlm3u8)<=0)
    {
        help();
        exit(0);
    }

	FILE * m3u8h = fopen(m3u8, "wb");
	if (m3u8h) {
		fclose(m3u8h);
	}

    curl_global_init(CURL_GLOBAL_DEFAULT);    
    baseurl = getbaseurl(urlm3u8);
    if(isDEBUG) cout << baseurl << endl;
    auto curl = curl_easy_init();
    if (curl) {
    	while (1) {
    		//curl_easy_setopt(curl, CURLOPT_URL, "https://api.github.com/repos/whoshuu/cpr/contributors?anon=true&key=value");
    		curl_easy_setopt(curl, CURLOPT_URL, urlm3u8);
    		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
    		//curl_easy_setopt(curl, CURLOPT_USERPWD, "user:pass");
    		curl_easy_setopt(curl, CURLOPT_USERAGENT, g_useragent);
    		curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 50L);
    		curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
    		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 5000L);
            if(strlen(g_proxy)>0)
                curl_easy_setopt(curl, CURLOPT_PROXY, g_proxy);

    		string response_string;
    		string header_string;
    		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFunction);
    		curl_easy_setopt(curl, CURLOPT_WRITEDATA,  & response_string);
    		curl_easy_setopt(curl, CURLOPT_HEADERDATA,  & header_string);

    		char * url;
    		long response_code;
    		double elapsed;
    		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE,  & response_code);
    		curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME,  & elapsed);
    		curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL,  & url);

    		res = curl_easy_perform(curl);
    		if (res != CURLE_OK)
            {
    			if(isDEBUG) fprintf(stderr, "main curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    		}
            else 
            {
    			if(isDEBUG) cout << "main response_code " << response_code << endl;
    			//cout << "elapsed       " << elapsed << endl;
    			//cout << "effective url " << url << endl;
    			//cout << "response_string \n" << response_string << endl;
    			//cout << "header_string \n" << header_string << endl;

                if( response_code == 0 ) continue;
                if( response_code == 200 )
                {
                    // skip first ts
                    if(firstdl != 0)
                        gettsurl(response_string);
                    firstdl ++;
                }
                else
                {
                    if(isDEBUG) cout << "#EXT-X-ENDLIST\r\n" << endl;
                    putmsg("#EXT-X-ENDLIST");
                    if(isDEBUG) cout << "main response_code " << response_code << "\n" << endl;
                    break;
                };
    		}
            char exitflag = '\0';
            if (_kbhit())
            {
                exitflag = _getch();
                if (exitflag == 'q' || exitflag == 'Q')
                {
                    if(isDEBUG) cout << "#EXT-X-ENDLIST\r\n" << endl;
                    putmsg("#EXT-X-ENDLIST");
                    break;
                }
            }
    		usleep(1000 * 1000);
    		//sleep(1);
    	}
    }
    curl_easy_cleanup(curl);
    curl = NULL;
    curl_global_cleanup();
    if(isDEBUG) cout << "main exit" << endl;
    return 0;
}