//
//  NSImage+MGCropExtensions.h
//  ImageCropDemo
//
//  Created by Matt Gemmell on 16/03/2006.
//

#import <Cocoa/Cocoa.h>

@interface NSImage (ImageAdditions)

typedef enum {
    MGImageResizeCrop,
    MGImageResizeCropStart,
    MGImageResizeCropEnd,
    MGImageResizeScale
} MGImageResizingMethod;

- (NSImage*)imageByScalingProportionallyToSize:(NSSize)targetSize;
- (NSImage*)imageByScalingProportionallyToSize:(NSSize)targetSize background:(NSColor*)bk;

- (void)drawInRect:(NSRect)dstRect operation:(NSCompositingOperation)op fraction:(float)delta method:(MGImageResizingMethod)resizeMethod;
- (NSImage*)imageToFitSize:(NSSize)size method:(MGImageResizingMethod)resizeMethod;
- (NSImage*)imageCroppedToFitSize:(NSSize)size;
- (NSImage*)imageScaledToFitSize:(NSSize)size;

- (NSImageRep*)largestRepresentation; 
- (NSSize)sizeLargestRepresentation;

- (NSImage*)rotated:(int)angle;

@end
