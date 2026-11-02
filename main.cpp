#include <iostream>
#include <string>
#include <vector>

#include <stdlib.h>
#include <unistd.h>

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
bool optCopyClipboard    = false;

int  optImageSize = 0;

std::vector<std::string> optFiles;
std::vector<std::string> tempFiles;

std::string clipBuffer;

int exitCode = 0;
//=========================================================================================================================
std::string processImage(std::string inputFile, int optImageSize);
void browseTo(std::string url);
void init();
void copyToClipboard(std::string text);

bool usage()
{
	printf("usage: imguru [-odpilmbdhvc] [-s max_dimension] source_file ...\n");
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
    printf("-c  Also copy link to clipboard\n");
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
	printf("imguru v1.0.6 by Roland Rabien (figbug@gmail.com) %s.\n", __DATE__);
	
	return true;
}

static size_t outputCallback(void *ptr, size_t size, size_t nmemb, void *data)
{
	int sz = int (size * nmemb);
	
	std::string* output = (std::string*)data;
	
	char* mem = new char[sz+1];
	memcpy(mem, ptr, sz);
	mem[sz] = 0;
	
	*output += mem;
	delete[] mem;
	
	return sz;
}

std::string getVal(std::string xml, std::string key)
{
	TiXmlDocument doc;
	doc.Parse(xml.c_str());
	
	TiXmlElement* root = doc.RootElement();
	if (root)
	{
		TiXmlElement* child = root->FirstChildElement(key.c_str());
        if (child && child->GetText())
            return child->GetText();
    }
	
	return "";	
}

// Returns the error message from an imgur api response, or "" if it succeeded.
std::string errorFromXml(std::string xml)
{
	TiXmlDocument doc;
	doc.Parse(xml.c_str());
	
	TiXmlElement* root = doc.RootElement();
	if (root)
	{
		const char* success = root->Attribute("success");
        if (success && std::string(success) == "1")
            return "";
        
        // <error> holds <code>, <message>, <type> and <exception> children
        TiXmlElement* error = root->FirstChildElement("error");
        if (error)
        {
            TiXmlElement* message = error->FirstChildElement("message");
            if (message && message->GetText())
                return message->GetText();
        }
	}
	
	return "Unexpected response from imgur.com";
}

std::string uploadImage(std::string filename, std::string& err)
{
	CURL *curl;
	CURLcode res;
	
	struct curl_httppost *formpost = NULL;
	struct curl_httppost *lastptr  = NULL;
    struct curl_slist *headers     = NULL;

	std::string output;
		
    curl = curl_easy_init();
    if (!curl)
    {
        err = "Failed to initialise curl";
        return "";
    }
    
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
		if (res != CURLE_OK)
			err = curl_easy_strerror(res);
		
		curl_easy_cleanup(curl);
		
		curl_formfree(formpost);
		curl_slist_free_all(headers);
	}

	return output;
}

int parseCmdLine(int argc, char* const argv[])
{
	opterr = 0;

	int c;	
	while ((c = getopt (argc, argv, "hvodpilmbcds:")) != -1)
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
            case 'c': optCopyClipboard    = true; break;
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
		bool found = false;
		char* path = realpath(argv[index], NULL);
        if (path)
        {
            struct stat buf;
            if (stat(path, &buf) == 0)
            {
                optFiles.push_back(std::string(path));
                found = true;
            }
            
            free(path);
        }
        
        if (!found)
        {
            fprintf(stderr, "No such file: %s\n", argv[index]);
            exitCode = 1;
        }
	}
	return 0;	
}

// Formats fmt and appends the result to s
void spf(std::string &s, const std::string fmt, ...)
{
    va_list marker;
    
    va_start(marker, fmt);
    int n = vsnprintf(NULL, 0, fmt.c_str(), marker);
    va_end(marker);
    
    if (n <= 0)
        return;
    
    std::vector<char> buf(n + 1);
    
    va_start(marker, fmt);
    vsnprintf(&buf[0], buf.size(), fmt.c_str(), marker);
    va_end(marker);
    
    s.append(&buf[0], n);
}

bool userOutput(std::string xml)
{
    // The upload response already carries everything we need to display
    std::string id   = getVal(xml, "id");
    std::string link = getVal(xml, "link");
    
    if (id.length() == 0 || link.length() == 0)
        return false;
    
    std::string output;
    
	if (optDirect)
		spf(output, "%s\n", link.c_str());
	if (optImagePage)
		spf(output, "http://imgur.com/%s\n", id.c_str());
	if (optHtmlImage)
		spf(output, "<img src=\"%s\" alt=\"Hosted by imgur.com\" />\n", link.c_str());
	if (optHtmlLink)
		spf(output, "<a href=\"%s\" title=\"Hosted by imgur.com\">%s</a>\n", link.c_str(), link.c_str());
	if (optMessageBoard)
		spf(output, "[IMG]%s[/IMG]\n", link.c_str());
	if (optMessageBoardLink)
		spf(output, "[URL=%s][IMG]%s[/IMG][/URL]\n", link.c_str(), link.c_str());
	if (optOpenBrowser)
    {
        std::string url = "http://imgur.com/" + id;
		browseTo(url);
    }
    if (output.length() > 0)
        puts(output.c_str());
    if (optCopyClipboard && output.length() > 0)
        clipBuffer += output;
    
	return true;
}

int main (int argc, char* const argv[]) 
{
	init();
	
	if (argc == 1 && usage())
		return 0;
	if (parseCmdLine(argc, argv) != 0)
		return 1;
	if (optShowHelp && showHelp())
		return 0;
	if (optShowVersion && showVersion())
		return 0;
    
    curl_global_init(CURL_GLOBAL_ALL);
	
	for (int i = 0; i < optFiles.size(); i++)
	{
		std::string processed = processImage(optFiles[i], optImageSize);
		if (processed == "")
		{
			fprintf(stderr, "Could not read image %s.\n", optFiles[i].c_str());
			exitCode = 1;
			continue;
		}
		
		if (processed != optFiles[i])
			tempFiles.push_back(processed);
		
        std::string err;
		std::string xml = uploadImage(processed, err);
		
		if (err.length() == 0)
			err = errorFromXml(xml);
		
		if (err.length() > 0 || !userOutput(xml))
		{
			fprintf(stderr, "Upload failed for %s.\n%s\n", optFiles[i].c_str(), err.c_str());
			exitCode = 1;
		}
	}
    
    if (clipBuffer.length() > 0)
        copyToClipboard(clipBuffer);
	
	for (int i = 0; i < tempFiles.size(); i++)
		remove(tempFiles[i].c_str());
	
    return exitCode;
}
