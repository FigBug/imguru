#include <iostream>
#include <string>
#include <vector>

#include <stdlib.h>

#include <curl/curl.h>
#include <curl/easy.h>

#include <sys/stat.h>

#include "tinyxml/tinyxml.h"

//=========================================================================================================================
bool optDirect           = false;
bool optImagePage        = false;
bool optHtmlImage        = false;
bool optHtmlLink         = false;
bool optMessageBoard     = false;
bool optMessageBoardLink = false;
bool optOpenBrowser      = false;
bool optShowHelp         = false;
bool optShowVersion      = false;

int  optImageSize = 0;

std::vector<std::string> optFiles;
std::vector<std::string> tempFiles;
//=========================================================================================================================
std::string processImage(std::string inputFile, int optImageSize);
void browseTo(std::string url);
void init();

bool usage()
{
	printf("usage: imguru [-odpilmbdhv] [-s max_dimension] source_file ...\n");
	return true;
}

bool showHelp()
{
	printf("usage: imguru [-odpilmbdhv] [-s max_dimension] source_file ...\n");
	printf("Uploads one of more images to imgur.com. Supports all image formats supported by Mac OS X and will resize and convert to JPEG before uploading.\n\n");
	printf("-d  Display direct link to image (default)\n");
	printf("-p  Display link to image page\n");
	printf("-i  Display HTML image\n");
	printf("-l  Display HTML link\n");
	printf("-m  Display message board image code\n");
	printf("-b  Display message board link code\n");
	printf("-o  Open uploaded images in browser\n");
	printf("-s  Resize image to have maximum dimension of max_dimension\n");
	printf("-h  Display help\n");
	printf("-v  Display version\n");
	return true;
}

bool showVersion()
{
	printf("imguru v1.0.4 by Roland Rabien (figbug@gmail.com) %s.\n", __DATE__);
	
	return true;
}

static size_t outputCallback(void *ptr, size_t size, size_t nmemb, void *data)
{
	int sz = size * nmemb;
	
	std::string* output = (std::string*)data;
	
	char* mem = new char[sz+1];
	memcpy(mem, ptr, sz);
	mem[sz] = 0;
	
	*output += mem;
	delete[] mem;
	
	return sz;
}

std::string imageIdFromXml(std::string xml)
{
	TiXmlDocument doc;
	doc.Parse(xml.c_str());
	
	TiXmlElement* root = doc.RootElement();
	if (root)
	{
		TiXmlElement* id = root->FirstChildElement("id");
        if (id)
            return id->GetText();
	}
	
	return "";

}

std::string getImageInfo(std::string id)
{
	CURL *curl;
	CURLcode res;
	
    struct curl_slist *headers     = NULL;
    
	std::string output;
    
    curl = curl_easy_init();
    
    headers = curl_slist_append(headers, "Authorization: Client-ID 51f229880e3ea84");
    
	if (curl)
	{
        std::string url = std::string("https://api.imgur.com/3/image/") + id + "/.xml";
        
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, outputCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&output);
		curl_easy_setopt(curl, CURLOPT_USERAGENT, "penis-browser/1.1");
		
		res = curl_easy_perform(curl);
		
		curl_easy_cleanup(curl);		
	}
    
	return output;
}

std::string uploadImage(std::string filename)
{
	CURL *curl;
	CURLcode res;
	
	struct curl_httppost *formpost = NULL;
	struct curl_httppost *lastptr  = NULL;
    struct curl_slist *headers     = NULL;

	std::string output;
		
    curl = curl_easy_init();
    
	curl_formadd(&formpost,
				 &lastptr,
				 CURLFORM_COPYNAME, "image",
				 CURLFORM_FILE, filename.c_str(),
				 CURLFORM_END);
    
    headers = curl_slist_append(headers, "Authorization: Client-ID 51f229880e3ea84");
		
	if (curl)
	{
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
		curl_easy_setopt(curl, CURLOPT_URL, "https://api.imgur.com/3/image.xml");
		curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);
		
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, outputCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&output);
		curl_easy_setopt(curl, CURLOPT_USERAGENT, "penis-browser/1.1");
		
		res = curl_easy_perform(curl);
		
		curl_easy_cleanup(curl);
		
		curl_formfree(formpost);
	}

	return imageIdFromXml(output);
}

int parseCmdLine(int argc, char* const argv[])
{
	opterr = 0;

	int c;	
	while ((c = getopt (argc, argv, "hvodpilmbds:")) != -1)
	{
		switch (c)
		{
			case 'd': optDirect           = true; break;
			case 'p': optImagePage        = true; break;
			case 'i': optHtmlImage        = true; break;
			case 'l': optHtmlLink         = true; break;
			case 'm': optMessageBoard     = true; break;
			case 'b': optMessageBoardLink = true; break;
			case 'o': optOpenBrowser      = true; break;
			case 'h': optShowHelp         = true; break;
			case 'v': optShowVersion      = true; break;
			case 's': optImageSize = atoi(optarg); break;
			case '?':
				if (optopt == 'c')
					fprintf (stderr, "Option -%c requires an argument.\n", optopt);
				else if (isprint (optopt))
					fprintf (stderr, "Unknown option `-%c'.\n", optopt);
				else
					fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
				return 1;
			default:
				abort ();
		}
	}
		
	if (!optDirect && !optImagePage && !optHtmlImage && !optHtmlLink && !optMessageBoard && !optMessageBoardLink)
		optDirect = true;
	
	for (int index = optind; index < argc; index++)
	{
		char* path = realpath(argv[index], NULL);
        if (path)
        {
            struct stat buf;
            if (stat(path, &buf) == 0)
                optFiles.push_back(std::string(path));
            
            free(path);
        }
	}
	return 0;	
}

std::string getVal(std::string xml, std::string key)
{
	TiXmlDocument doc;
	doc.Parse(xml.c_str());
	
	TiXmlElement* root = doc.RootElement();
	if (root)
	{
		TiXmlElement* child = root->FirstChildElement(key.c_str());
        if (child)
            return child->GetText();
    }
	
	return "";	
}

bool userOutput(std::string id)
{
    if (id.length() == 0)
        return false;
    
    std::string xml = getImageInfo(id);
    
	if (optDirect)
		printf("%s\n", getVal(xml, "link").c_str());
	if (optImagePage)
		printf("http://imgur.com/%s\n", id.c_str());
	if (optHtmlImage)
		printf("<img src=\"%s\" alt=\"Hosted by imgur.com\" />\n", getVal(xml, "link").c_str());
	if (optHtmlLink)
		printf("<a href=\"%s\" title=\"Hosted by imgur.com\">%s</a>\n", getVal(xml, "link").c_str(), getVal(xml, "link").c_str());
	if (optMessageBoard)
		printf("[IMG]%s[/IMG]\n", getVal(xml, "link").c_str());
	if (optMessageBoardLink)
		printf("[URL=%s][IMG]%s[/IMG][/URL]\n", getVal(xml, "link").c_str(), getVal(xml, "link").c_str());
	if (optOpenBrowser)
		browseTo("http://imgur.com/" + id);
	
	return true;
}

int main (int argc, char* const argv[]) 
{
	init();
	
	if (argc == 1 && usage())
		return 0;
	if (parseCmdLine(argc, argv) != 0)
		return 0;
	if (optShowHelp && showHelp())
		return 0;
	if (optShowVersion && showVersion())
		return 0;
    
    curl_global_init(CURL_GLOBAL_ALL);
	
	for (int i = 0; i < optFiles.size(); i++)
	{
		std::string processed = processImage(optFiles[i], optImageSize);
		if (processed == "") 
			continue;
		
		if (processed != optFiles[i])
			tempFiles.push_back(processed);
		
		std::string output = uploadImage(processed.c_str());
		if (!userOutput(output))
			fprintf(stderr, "Upload failed for %s.\n", optFiles[i].c_str());
	}
	
	for (int i = 0; i < tempFiles.size(); i++)
		remove(tempFiles[i].c_str());
	
    return 0;
}
