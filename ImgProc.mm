#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>

#import "NSImageAdditions.h"

#include <string>
#include <vector>

void init()
{
	NSApplicationLoad();
}

void browseTo(std::string url)
{
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	[[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:[NSString stringWithUTF8String:url.c_str()]]];
	[pool release];
}

std::string processImage(std::string inputFile, int optImageSize)
{
	if (optImageSize == 0)
		optImageSize = 10000;	
	
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	
	NSArray* supportedTypes = [NSImage imageTypes];
	
	NSString* path = [NSString stringWithUTF8String: inputFile.c_str()];
	
	NSError* error;
	NSString* fileType = [[NSWorkspace sharedWorkspace] typeOfFile:path error: &error];
	
	if (![supportedTypes containsObject:fileType])
		return "";
	
	NSDictionary* attr = [[NSFileManager defaultManager] attributesOfItemAtPath:path error: &error];
	bool size = ([[attr objectForKey: NSFileSize] longLongValue] > 2 * 1024 * 1024);
			
	bool type = (![fileType isEqual:@"com.compuserve.gif"] && ![fileType isEqual:@"public.png"] && ![fileType isEqual:@"public.jpeg"]);
	
	NSImage* img = [[NSImage alloc] initByReferencingFile:path];
	
	NSSize sz = [img sizeLargestRepresentation];
	bool dims =  (sz.width > optImageSize || sz.height > optImageSize);
	
	std::string res = inputFile;
	
	if (size || type || dims)
	{
		if (dims)
		{
			NSImage* newImg = [img imageByScalingProportionallyToSize:NSMakeSize(optImageSize,optImageSize)];
			[img release];
			img = [newImg retain];
		}
		
		NSBitmapImageFileType newType = NSJPEGFileType;
		
		if ([fileType isEqual:@"com.compuserve.gif"])
			newType = NSGIFFileType;
		if ([fileType isEqual:@"public.png"])
			newType = NSPNGFileType;
		
		NSBitmapImageRep* bits = [NSBitmapImageRep imageRepWithData: [img TIFFRepresentation]];			
		NSData* data = [bits representationUsingType: NSPNGFileType properties: nil];
		
		NSString* tempTemplate = [NSString stringWithFormat: @"%@XXXXXXXX", NSTemporaryDirectory()];
		
		char tempFile[PATH_MAX];
		strcpy(tempFile, [tempTemplate UTF8String]);
		
		switch (newType)
		{
			case NSJPEGFileType: strcat(tempFile, ".jpg"); break;
			case NSGIFFileType:  strcat(tempFile, ".gif"); break;
			case NSPNGFileType:  strcat(tempFile, ".png"); break;
		}
		
		mktemp(tempFile);
		
		FILE* fp = fopen(tempFile, "w");
		if (fp)
		{
			fwrite([data bytes], 1, [data length], fp);
			fclose(fp);
			
			res = std::string(tempFile);
		}			
	}
	
	[img release];
	
	[pool release];

	return res;
}