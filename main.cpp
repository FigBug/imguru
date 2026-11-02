#include <iostream>
#include <string>
#include <vector>
#include <map>

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <time.h>

#include <curl/curl.h>
#include <curl/easy.h>

#include <sys/stat.h>

#include "tinyxml/tinyxml.h"

//=========================================================================================================================
// Imgur no longer registers new api clients, so everyone shares this one.
static const char* kClientId  = "51f229880e3ea84";
static const char* kUserAgent = "penis-browser/1.1";

// Signing in needs the client secret, which is deliberately not in this repo.
// Release builds get it from a generated Secret.h (see ci/mac/build.sh); for a
// local build put client_secret= in ~/.imguru or set IMGUR_CLIENT_SECRET.
// Without it uploads still work, they are just anonymous.
#if __has_include("Secret.h")
 #include "Secret.h"
#endif

#ifndef IMGUR_CLIENT_SECRET
 #define IMGUR_CLIENT_SECRET ""
#endif

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
bool optAuthenticate     = false;
bool optSignOut          = false;
bool optShowAccount      = false;
bool optAnonymous        = false;

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
	printf("usage: imguru [-odpilmbdhvcanwx] [-s max_dimension] source_file ...\n");
	return true;
}

bool showHelp()
{
	printf("usage: imguru [-odpilmbdhvcanwx] [-s max_dimension] source_file ...\n");
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
	printf("-a  Sign in to an imgur account, uploads then go to it\n");
	printf("-n  Upload anonymously even when signed in\n");
	printf("-w  Show which account imguru is signed in to\n");
	printf("-x  Sign out\n");
	printf("-h  Display help\n");
	printf("-v  Display version\n");
	return true;
}

bool showVersion()
{
	printf("imguru v1.0.7 by Roland Rabien (figbug@gmail.com) %s.\n", __DATE__);
	
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

//=========================================================================================================================
std::string configPath()
{
    const char* home = getenv("HOME");
    if (home == NULL)
        return "";
    
    return std::string(home) + "/.imguru";
}

std::string trimmed(std::string s)
{
    size_t first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return "";
    
    return s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1);
}

std::map<std::string, std::string> readConfig()
{
    std::map<std::string, std::string> config;
    
    std::string path = configPath();
    if (path.length() == 0)
        return config;
    
    FILE* f = fopen(path.c_str(), "r");
    if (f == NULL)
        return config;
    
    char line[2048];
    while (fgets(line, sizeof(line), f) != NULL)
    {
        std::string s = trimmed(line);
        if (s.length() == 0 || s[0] == '#')
            continue;
        
        size_t eq = s.find('=');
        if (eq == std::string::npos || eq == 0)
            continue;
        
        config[trimmed(s.substr(0, eq))] = trimmed(s.substr(eq + 1));
    }
    
    fclose(f);
    
    return config;
}

bool writeConfig(std::map<std::string, std::string> config)
{
    std::string path = configPath();
    if (path.length() == 0)
    {
        fprintf(stderr, "HOME is not set, nowhere to save credentials.\n");
        return false;
    }
    
    // The file holds an access token, so keep it readable by its owner only
    int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd < 0)
    {
        fprintf(stderr, "Could not write %s.\n", path.c_str());
        return false;
    }
    
    FILE* f = fdopen(fd, "w");
    if (f == NULL)
    {
        close(fd);
        return false;
    }
    
    std::map<std::string, std::string>::iterator it;
    for (it = config.begin(); it != config.end(); ++it)
        if (it->second.length() > 0)
            fprintf(f, "%s=%s\n", it->first.c_str(), it->second.c_str());
    
    fclose(f);
    chmod(path.c_str(), S_IRUSR | S_IWUSR);
    
    return true;
}

std::string cfgGet(std::map<std::string, std::string>& config, std::string key)
{
    std::map<std::string, std::string>::iterator it = config.find(key);
    return (it == config.end()) ? "" : it->second;
}

// Minimal reader for the flat json the oauth2 endpoints return
std::string jsonVal(std::string json, std::string key)
{
    std::string needle = "\"" + key + "\"";
    
    size_t pos = json.find(needle);
    if (pos == std::string::npos)
        return "";
    
    pos = json.find(':', pos + needle.length());
    if (pos == std::string::npos)
        return "";
    
    pos++;
    while (pos < json.length() && isspace((unsigned char)json[pos]))
        pos++;
    
    if (pos < json.length() && json[pos] == '"')
    {
        pos++;
        
        size_t end = json.find('"', pos);
        if (end == std::string::npos)
            return "";
        
        return json.substr(pos, end - pos);
    }
    
    size_t end = pos;
    while (end < json.length() && json[end] != ',' && json[end] != '}')
        end++;
    
    return trimmed(json.substr(pos, end - pos));
}

std::string clientSecret()
{
    std::map<std::string, std::string> config = readConfig();
    
    std::string secret = cfgGet(config, "client_secret");
    if (secret.length() > 0)
        return secret;
    
    const char* env = getenv("IMGUR_CLIENT_SECRET");
    if (env != NULL && *env != 0)
        return env;
    
    return IMGUR_CLIENT_SECRET;
}

std::string urlEncode(std::string s)
{
    std::string encoded;
    
    CURL* curl = curl_easy_init();
    if (curl)
    {
        char* escaped = curl_easy_escape(curl, s.c_str(), int (s.length()));
        if (escaped)
        {
            encoded = escaped;
            curl_free(escaped);
        }
        
        curl_easy_cleanup(curl);
    }
    
    return encoded;
}

std::string httpPost(std::string url, std::string fields, std::string& err)
{
    std::string output;
    
    CURL* curl = curl_easy_init();
    if (!curl)
    {
        err = "Failed to initialise curl";
        return "";
    }
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, fields.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, outputCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&output);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, kUserAgent);
    
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK)
        err = curl_easy_strerror(res);
    
    curl_easy_cleanup(curl);
    
    return output;
}

// Stores the tokens from an /oauth2/token response. Returns false and fills
// err if the response was not a successful one.
bool saveTokens(std::string json, std::string& err)
{
    std::string token = jsonVal(json, "access_token");
    if (token.length() == 0)
    {
        err = jsonVal(json, "error");
        if (err.length() == 0)
            err = "Unexpected response from imgur.com";
        
        return false;
    }
    
    std::map<std::string, std::string> config = readConfig();
    
    config["access_token"] = token;
    
    // Imgur rotates the refresh token, but only overwrite what it actually
    // sent, otherwise a response that omits either one would discard ours
    std::string refresh = jsonVal(json, "refresh_token");
    if (refresh.length() > 0)
        config["refresh_token"] = refresh;
    
    std::string user = jsonVal(json, "account_username");
    if (user.length() > 0)
        config["account_username"] = user;
    
    long expiresIn = atol(jsonVal(json, "expires_in").c_str());
    if (expiresIn > 0)
    {
        char expiresAt[32];
        snprintf(expiresAt, sizeof(expiresAt), "%ld", long (time(NULL)) + expiresIn);
        config["expires_at"] = expiresAt;
    }
    else
    {
        config.erase("expires_at");
    }
    
    if (!writeConfig(config))
    {
        err = "Could not save credentials";
        return false;
    }
    
    return true;
}

// Trades the pin the user pastes in for an access token
bool authenticate()
{
    std::string secret = clientSecret();
    if (secret.length() == 0)
    {
        fprintf(stderr, "No client secret available, so imguru cannot sign in.\n");
        fprintf(stderr, "Add client_secret=... to %s, or set IMGUR_CLIENT_SECRET.\n", configPath().c_str());
        return false;
    }
    
    std::string url = std::string("https://api.imgur.com/oauth2/authorize?client_id=") + kClientId + "&response_type=pin";
    
    printf("Opening %s\n", url.c_str());
    browseTo(url);
    
    printf("Allow access, then paste the PIN here: ");
    fflush(stdout);
    
    char buf[256];
    if (fgets(buf, sizeof(buf), stdin) == NULL)
    {
        fprintf(stderr, "\nNo PIN entered.\n");
        return false;
    }
    
    std::string pin = trimmed(buf);
    if (pin.length() == 0)
    {
        fprintf(stderr, "No PIN entered.\n");
        return false;
    }
    
    std::string fields = std::string("client_id=") + urlEncode(kClientId)
                       + "&client_secret=" + urlEncode(secret)
                       + "&grant_type=pin"
                       + "&pin=" + urlEncode(pin);
    
    std::string err;
    std::string json = httpPost("https://api.imgur.com/oauth2/token", fields, err);
    
    if (err.length() == 0)
        saveTokens(json, err);
    
    if (err.length() > 0)
    {
        fprintf(stderr, "Sign in failed.\n%s\n", err.c_str());
        return false;
    }
    
    std::map<std::string, std::string> config = readConfig();
    std::string user = cfgGet(config, "account_username");
    
    printf("Signed in%s%s. Uploads will go to your account.\n",
           user.length() > 0 ? " as " : "", user.c_str());
    
    return true;
}

// Returns an account access token, refreshing it first if it has expired.
// An empty string means upload anonymously.
std::string accessToken()
{
    std::map<std::string, std::string> config = readConfig();
    
    std::string token = cfgGet(config, "access_token");
    if (token.length() == 0)
        return "";
    
    std::string expiresAt = cfgGet(config, "expires_at");
    if (expiresAt.length() == 0 || time(NULL) < atol(expiresAt.c_str()) - 60)
        return token;
    
    std::string refresh = cfgGet(config, "refresh_token");
    std::string secret  = clientSecret();
    
    if (refresh.length() == 0 || secret.length() == 0)
    {
        fprintf(stderr, "Your imgur sign in has expired, run imguru -a to sign in again.\n");
        return "";
    }
    
    std::string fields = std::string("client_id=") + urlEncode(kClientId)
                       + "&client_secret=" + urlEncode(secret)
                       + "&grant_type=refresh_token"
                       + "&refresh_token=" + urlEncode(refresh);
    
    std::string err;
    std::string json = httpPost("https://api.imgur.com/oauth2/token", fields, err);
    
    if (err.length() == 0)
        saveTokens(json, err);
    
    if (err.length() > 0)
    {
        fprintf(stderr, "Could not refresh your imgur sign in, run imguru -a to sign in again.\n%s\n", err.c_str());
        return "";
    }
    
    config = readConfig();
    
    return cfgGet(config, "access_token");
}

bool showAccount()
{
    std::map<std::string, std::string> config = readConfig();
    
    std::string token = cfgGet(config, "access_token");
    if (token.length() == 0)
    {
        printf("Not signed in, uploads are anonymous.\n");
        return true;
    }
    
    std::string user = cfgGet(config, "account_username");
    printf("Signed in%s%s.\n", user.length() > 0 ? " as " : "", user.c_str());
    
    std::string expiresAt = cfgGet(config, "expires_at");
    if (expiresAt.length() > 0)
    {
        time_t expires = time_t (atol(expiresAt.c_str()));
        
        if (time(NULL) >= expires)
            printf("Access token expired %s", ctime(&expires));
        else
            printf("Access token expires %s", ctime(&expires));
        
        if (cfgGet(config, "refresh_token").length() > 0)
            printf("It will be refreshed automatically.\n");
    }
    
    return true;
}

bool signOut()
{
    std::map<std::string, std::string> config = readConfig();
    
    if (cfgGet(config, "access_token").length() == 0)
    {
        printf("Not signed in.\n");
        return true;
    }
    
    config.erase("access_token");
    config.erase("refresh_token");
    config.erase("account_username");
    config.erase("expires_at");
    
    if (!writeConfig(config))
        return false;
    
    printf("Signed out, uploads will be anonymous.\n");
    
    return true;
}

std::string uploadImage(std::string filename, std::string token, std::string& err)
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
    
    std::string auth = token.length() > 0
                     ? std::string("Authorization: Bearer ") + token
                     : std::string("Authorization: Client-ID ") + kClientId;
    
    headers = curl_slist_append(headers, auth.c_str());
		
	if (curl)
	{
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
		curl_easy_setopt(curl, CURLOPT_URL, "https://api.imgur.com/3/image.xml");
		curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);
		
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, outputCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&output);
		curl_easy_setopt(curl, CURLOPT_USERAGENT, kUserAgent);
		
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
	while ((c = getopt (argc, argv, "hvodpilmbcdanwxs:")) != -1)
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
            case 'a': optAuthenticate     = true; break;
            case 'n': optAnonymous        = true; break;
            case 'w': optShowAccount      = true; break;
            case 'x': optSignOut          = true; break;
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
    
    if (optSignOut)
        return signOut() ? 0 : 1;
    if (optAuthenticate)
        return authenticate() ? 0 : 1;
    if (optShowAccount)
        return showAccount() ? 0 : 1;
    
    // An empty token uploads anonymously
    std::string token = optAnonymous ? "" : accessToken();
	
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
		std::string xml = uploadImage(processed, token, err);
		
		if (err.length() == 0)
			err = errorFromXml(xml);
		
		if (err.length() > 0 || !userOutput(xml))
		{
			fprintf(stderr, "Upload failed for %s.\n%s\n", optFiles[i].c_str(), err.c_str());
			
			if (token.length() > 0)
				fprintf(stderr, "If you are signed in, your access token may no longer be "
				                "valid. Try imguru -a, or imguru -n to upload anonymously.\n");
			
			exitCode = 1;
		}
	}
    
    if (clipBuffer.length() > 0)
        copyToClipboard(clipBuffer);
	
	for (int i = 0; i < tempFiles.size(); i++)
		remove(tempFiles[i].c_str());
	
    return exitCode;
}
