//
//  osutil.m
//  imgur
//
//  Created by Roland Rabien on 2014-09-19.
//
//

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#include <string>

void copyToClipboard(std::string text)
{
    NSString* string = [NSString stringWithUTF8String:text.c_str()];
    
    NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard declareTypes:[NSArray arrayWithObject:NSStringPboardType] owner:nil];
    [pasteboard setString:string forType:NSStringPboardType];
}