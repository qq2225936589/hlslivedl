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
#include <list>
#include <pthread.h>
using namespace std;

static char m3u8[1024];
static char g_useragent[1024];
static char g_proxy[100];
static char g_maxlen[50];
static double g_maxduration = 0.0;
//static size_t totalsize = 0;
static unsigned long long int totalsize = 0;
static char strtotalsize[100];

static string baseurl;
static int isDEBUG = 0;

static list<pthread_t>listpthreads;
pthread_mutex_t lock;

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
    char idxmsg[512];
} targ;

size_t writeFunction(void *ptr, size_t size, size_t nmemb, string* data) {
    data->append((char*) ptr, size * nmemb);
    return size * nmemb;
}
size_t writeFunction2(void *ptr, size_t size, size_t nmemb, string* data) {
    data->append((char*) ptr, size * nmemb);
    return size * nmemb;
}
static void deleteNode(pthread_t tid)
{
    list<pthread_t>::iterator it;
    for (it = listpthreads.begin(); it != listpthreads.end(); it++)
    {
        if (tid == *it)
        {
            pthread_mutex_lock(&lock);
            listpthreads.erase(it);
            pthread_mutex_unlock(&lock);
        }
    }
}

static void waitThreads()
{
    list<pthread_t>::iterator it;
    string str = " ";
    if(listpthreads.size()<=0) return;
    for (it = listpthreads.begin(); it != listpthreads.end(); it++)
    {
        str += std::to_string(*it);
        if(it!=listpthreads.end())
            str += " ";
    }

    cout << "Waiting for the end of download threads. [" << listpthreads.size() << " =" << str << "]" << endl;
    for (it = listpthreads.begin(); it != listpthreads.end(); it++)
    {
        //cout << "      thread ID " << *it << endl;
        pthread_join(*it, NULL);
    }
}

static void * downts(void *arg)
{
    targ *ta= (targ *)arg;
    string fullurl = ta->url;;
    string fn = ta->fn;
    string idxmsg = ta->idxmsg;
    string statusmsg = " [OK]";
    char recvsize[20];
    //sprintf(recvsize,"          ");
    sprintf(recvsize,"----------");
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
            //cout << "*** failed [" << fn << "] ***" <<endl;
            statusmsg = " [Failed]";
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
            unsigned long long int rsize = response_string.size();
            if( rsize > 0 )
            {
                //cout << fn << endl;
                FILE *tsh = fopen(fn.c_str(), "wb");
                if(tsh)
                {
                    fwrite(response_string.c_str(), 1, rsize, tsh);
                    fclose(tsh);
                }
                totalsize += rsize;
                if(totalsize<1024)                sprintf(strtotalsize,"%10ld   ", totalsize);
                else if(totalsize<1024*1024)      sprintf(strtotalsize,"%10.03f KB", (double)totalsize/1024);
                else if(totalsize<1024*1024*1024) sprintf(strtotalsize,"%10.03f MB", (double)totalsize/1024/1024);
                else                              sprintf(strtotalsize,"%10.03f GB", (double)totalsize/1024/1024/1024);
                
                if(rsize<1024)                    sprintf(recvsize,"%7ld   ", rsize);
                else if(rsize<1024*1024)          sprintf(recvsize,"%7.02f KB", (double)rsize/1024);
                else if(rsize<1024*1024*1024)     sprintf(recvsize,"%7.02f MB", (double)rsize/1024/1024);
                else                              sprintf(recvsize,"%7.02f MB", (double)rsize/1024/1024/1024);

            }
            else
            {
                //cout << "*** response error [" << fn << "] ***" <<endl;
                statusmsg = " [Response error]";
                remove(fn.c_str());
            }
    	}
    }
    cout << " " << idxmsg << " " << recvsize << strtotalsize << " " << fn << statusmsg << endl;
    curl_easy_cleanup(curl);
    curl = NULL;
    if(isDEBUG) cout << "pthread exit" << endl;
    deleteNode(pthread_self());
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
    return ret;
}

static double duration = 0.0;
static char strduration[512];
static unsigned int dlidx = 1;
static int isMAX = 0;
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
    if(g_maxduration != 0.0 && duration >= g_maxduration)
        isMAX = 1;
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
static unsigned int dladdcount = 0;
static unsigned int checknodowncount = 0;

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
            
            if(newfn.size()<=4)
            {
                char tmp[50];
                sprintf(tmp, "%05d", atoi(newfn.c_str()));
                newfn = tmp;
            }
            newfn = newfn + ".ts";
            
        	if (access(newfn.c_str(), 0)) {
                getlen(extinfo);
        		//cout << " " << strduration << " " << strtotalsize << " " << newfn << endl;
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
        		strcpy(ta.idxmsg, strduration);
                pthread_create( & thread, NULL, downts,  & ta);
                pthread_mutex_lock(&lock);
                listpthreads.push_back(thread);
                pthread_mutex_unlock(&lock);
                dladdcount ++;
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
    cout << "    -t set duration seconds" << endl;
    cout << "    -d debug mode" << endl;
    cout << "    Press [Q] to stop download" << endl;
    cout << "    Version 1.0.9 by NLSoft 2020.07" << endl;
}

int main(int argc, char** argv) {
    CURLcode res;
    static char urlm3u8[1024];
    memset(urlm3u8,0,1024);
    memset(g_proxy,0,100);
    memset(strduration,0,100);
    memset(strtotalsize,0,100);
    strcpy(strtotalsize, "             ");

    if(argc == 1)
    {
        help();
        exit(0);
    }
    strcpy(m3u8, "_index.m3u8");
    strcpy(g_useragent, "Mozilla/5.0 (iPhone; CPU iPhone OS 12_0 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/12.0 Mobile/15E148 Safari/604.1");
    
    int option_index = 0;
    char * user_name = NULL;
    while ((option_index = getopt(argc, argv, "p:o:u:i:t:d")) != -1) {
        switch (option_index) {
        case 'o':
            strcpy(m3u8, optarg);
            if(isDEBUG) printf("output filename is %s\n", m3u8);
            break;
        case 'u':
            strcpy(g_useragent, optarg);
            if(isDEBUG) printf("useragent is %s\n", g_useragent);
            break;
        case 'p':
            strcpy(g_proxy, optarg);
            if(isDEBUG) printf("proxy is %s\n", g_proxy);
            break;
        case 'i':
            strcpy(urlm3u8, optarg);
            if(isDEBUG) printf("m3u8 url is %s\n", urlm3u8);
            break;
        case 't':
            strcpy(g_maxlen, optarg);
            g_maxduration = atof(g_maxlen);
            if(isDEBUG) printf("g_maxlen is %s\n", g_maxlen);
            break;
        case 'd':
            isDEBUG = 1;
            if(isDEBUG) printf("select debug mode\n");
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
    if (pthread_mutex_init(&lock, NULL) != 0)
    {
        printf("mutex init failed\n");
        return 1;
    }
    curl_global_init(CURL_GLOBAL_DEFAULT);    
    baseurl = getbaseurl(urlm3u8);
    if(isDEBUG) cout << baseurl << endl;
    auto curl = curl_easy_init();
    if (curl) {
    	while (1) {
            dladdcount = 0;
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
                    waitThreads();
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
                    waitThreads();
                    if(isDEBUG) cout << "#EXT-X-ENDLIST\r\n" << endl;
                    putmsg("#EXT-X-ENDLIST");
                    break;
                }
            }
            
            if(dladdcount>0) checknodowncount = 0;
            else checknodowncount ++;
            //printf("dladdcount %d checknodowncount %ld\n", dladdcount, checknodowncount);
            
            // No files were downloaded in 60 seconds
            if(checknodowncount>=60 || isMAX)
            {
                waitThreads();
                if(isDEBUG) cout << "#EXT-X-ENDLIST\r\n" << endl;
                putmsg("#EXT-X-ENDLIST");
                break;
            }
    		usleep(1000 * 1000);
    		//sleep(1);
    	}
    }
    curl_easy_cleanup(curl);
    curl = NULL;
    curl_global_cleanup();
    pthread_mutex_destroy(&lock);
    if(isDEBUG) cout << "main exit" << endl;
    return 0;
}