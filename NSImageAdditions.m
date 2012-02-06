//
//  NSImage+MGCropExtensions.m
//  ImageCropDemo
//
//  Created by Matt Gemmell on 16/03/2006.
//

#import "NSImageAdditions.h"

@implementation NSImage (ImageAdditions)

- (NSImage*)imageByScalingProportionallyToSize:(NSSize)targetSize background:(NSColor*)bk
{
	NSImage* sourceImage = self;
	NSImage* newImage = nil;
	
	if ([sourceImage isValid])
	{
		NSSize imageSize = [self sizeLargestRepresentation];
		
		if (imageSize.width  <= targetSize.width &&
			imageSize.height <= targetSize.height)
		{
			[self setSize:imageSize];
			return self;
		}
		
		float scaleFactor  = 0.0;
		float scaledWidth  = targetSize.width;
		float scaledHeight = targetSize.height;
		
		float widthFactor  = targetSize.width / imageSize.width;
		float heightFactor = targetSize.height / imageSize.height;
		
		if ( widthFactor < heightFactor )
			scaleFactor = widthFactor;
		else
			scaleFactor = heightFactor;
		
		scaledWidth  = imageSize.width  * scaleFactor;
		scaledHeight = imageSize.height * scaleFactor;
		
		newImage = [[NSImage alloc] initWithSize:NSMakeSize(scaledWidth, scaledHeight)];
		
		NSRect thumbnailRect;
		thumbnailRect.origin      = NSMakePoint(0,0);
		thumbnailRect.size.width  = scaledWidth;
		thumbnailRect.size.height = scaledHeight;		
		
		[newImage lockFocus];
		[bk drawSwatchInRect:thumbnailRect];
		
		[[NSGraphicsContext currentContext] setImageInterpolation:NSImageInterpolationHigh];
				
		[sourceImage drawInRect: thumbnailRect
					   fromRect: NSZeroRect
					  operation: NSCompositeSourceOver
					   fraction: 1.0];
		
		[newImage unlockFocus];		
	}
	
	return [newImage autorelease];
}

- (NSImage*)imageByScalingProportionallyToSize:(NSSize)targetSize
{
	NSImage* sourceImage = self;
	NSImage* newImage = nil;
	
	if ([sourceImage isValid])
	{
		NSSize imageSize = [self sizeLargestRepresentation];
		
		if (imageSize.width  <= targetSize.width &&
			imageSize.height <= targetSize.height)
		{
			[self setSize:imageSize];
			return self;
		}
				
		float scaleFactor  = 0.0;
		float scaledWidth  = targetSize.width;
		float scaledHeight = targetSize.height;
		
		float widthFactor  = targetSize.width / imageSize.width;
		float heightFactor = targetSize.height / imageSize.height;
		
		if ( widthFactor < heightFactor )
			scaleFactor = widthFactor;
		else
			scaleFactor = heightFactor;
		
		scaledWidth  = imageSize.width  * scaleFactor;
		scaledHeight = imageSize.height * scaleFactor;
		
		newImage = [[NSImage alloc] initWithSize:NSMakeSize(scaledWidth, scaledHeight)];
		
		[newImage lockFocus];
		[[NSGraphicsContext currentContext] setImageInterpolation:NSImageInterpolationHigh];
		
		NSRect thumbnailRect;
		thumbnailRect.origin      = NSMakePoint(0,0);
		thumbnailRect.size.width  = scaledWidth;
		thumbnailRect.size.height = scaledHeight;
		
		[sourceImage drawInRect: thumbnailRect
					   fromRect: NSZeroRect
					  operation: NSCompositeSourceOver
					   fraction: 1.0];
		
		[newImage unlockFocus];
	}
	
	return [newImage autorelease];
}

- (void)drawInRect:(NSRect)dstRect operation:(NSCompositingOperation)op fraction:(float)delta method:(MGImageResizingMethod)resizeMethod
{
	float sourceWidth = [self sizeLargestRepresentation].width;
    float sourceHeight = [self sizeLargestRepresentation].height;
    float targetWidth = dstRect.size.width;
    float targetHeight = dstRect.size.height;
    BOOL cropping = !(resizeMethod == MGImageResizeScale);
    
    // Calculate aspect ratios
    float sourceRatio = sourceWidth / sourceHeight;
    float targetRatio = targetWidth / targetHeight;
    
    // Determine what side of the source image to use for proportional scaling
    BOOL scaleWidth = (sourceRatio <= targetRatio);
    // Deal with the case of just scaling proportionally to fit, without cropping
    scaleWidth = (cropping) ? scaleWidth : !scaleWidth;
    
    // Proportionally scale source image
    float scalingFactor, scaledWidth, scaledHeight;
    if (scaleWidth) {
        scalingFactor = 1.0 / sourceRatio;
        scaledWidth = targetWidth;
        scaledHeight = round(targetWidth * scalingFactor);
    } else {
        scalingFactor = sourceRatio;
        scaledWidth = round(targetHeight * scalingFactor);
        scaledHeight = targetHeight;
    }
    float scaleFactor = scaledHeight / sourceHeight;
    
    // Calculate compositing rectangles
    NSRect sourceRect;
    if (cropping) {
        float destX, destY;
        if (resizeMethod == MGImageResizeCrop) {
            // Crop center
            destX = round((scaledWidth - targetWidth) / 2.0);
            destY = round((scaledHeight - targetHeight) / 2.0);
        } else if (resizeMethod == MGImageResizeCropStart) {
            // Crop top or left (prefer top)
            if (scaleWidth) {
				// Crop top
				destX = round((scaledWidth - targetWidth) / 2.0);
				destY = round(scaledHeight - targetHeight);
            } else {
				// Crop left
                destX = 0.0;
				destY = round((scaledHeight - targetHeight) / 2.0);
            }
        } else if (resizeMethod == MGImageResizeCropEnd) {
            // Crop bottom or right
            if (scaleWidth) {
				// Crop bottom
				destX = 0.0;
				destY = 0.0;
            } else {
				// Crop right
				destX = round(scaledWidth - targetWidth);
				destY = round((scaledHeight - targetHeight) / 2.0);
            }
        }
        sourceRect = NSMakeRect(destX / scaleFactor, destY / scaleFactor, 
                                targetWidth / scaleFactor, targetHeight / scaleFactor);
    } else {
        sourceRect = NSMakeRect(0, 0, sourceWidth, sourceHeight);
		dstRect.origin.x += (targetWidth - scaledWidth) / 2.0;
		dstRect.origin.y += (targetHeight - scaledHeight) / 2.0;
		dstRect.size.width = scaledWidth;
		dstRect.size.height = scaledHeight;
    }
    
    [self drawInRect:dstRect fromRect:sourceRect operation:op fraction:delta];
}

- (NSImage *)imageToFitSize:(NSSize)size method:(MGImageResizingMethod)resizeMethod
{
    NSImage *result = [[NSImage alloc] initWithSize:size];
    
    // Composite image appropriately
    [result lockFocus];
    [[NSGraphicsContext currentContext] setImageInterpolation:NSImageInterpolationHigh];
	[self drawInRect:NSMakeRect(0,0,size.width,size.height) operation:NSCompositeSourceOver fraction:1.0 method:resizeMethod];
    [result unlockFocus];
    
    return [result autorelease];
}

- (NSImage *)imageCroppedToFitSize:(NSSize)size
{
    return [self imageToFitSize:size method:MGImageResizeCrop];
}

- (NSImage *)imageScaledToFitSize:(NSSize)size
{
    return [self imageToFitSize:size method:MGImageResizeScale];
}

- (NSImageRep*)largestRepresentation
{
	int area = 0;
	NSImageRep* largest = nil;
	
	for (NSImageRep* rep in [self representations])
	{
		int a = [rep pixelsWide] * [rep pixelsHigh];
		if (a > area)
		{
			area = a;
			largest = rep;
		}
	}
	return largest;
}

- (NSSize)sizeLargestRepresentation
{
	NSImageRep* rep = [self largestRepresentation];
	if (rep)		
		return NSMakeSize([rep pixelsWide], [rep pixelsHigh]);
	else
		return [self size];
}

- (NSImage*)rotated:(int)angle
/*{
	if (angle == 0 || ![self isValid])
		return self;
	
	NSSize beforeSize = [self size];
	
	NSSize afterSize = (angle == 90 || angle == -90) ? NSMakeSize(beforeSize.height, beforeSize.width) : beforeSize;
	
	NSAffineTransform* trans = [NSAffineTransform transform];
	[trans translateXBy:afterSize.width * 0.5 yBy:afterSize.height * 0.5];
	[trans rotateByDegrees:angle];
	[trans translateXBy:-beforeSize.width * 0.5 yBy:-beforeSize.height * 0.5];
	
	NSImage* newImage = [[NSImage alloc] initWithSize:afterSize];
		
	[newImage lockFocus];
	
	[trans set];
	[self drawAtPoint:NSZeroPoint fromRect:NSMakeRect(0, 0, beforeSize.width, beforeSize.height) operation:NSCompositeSourceOver fraction:1.0];
	
	[newImage unlockFocus];
	
	return [newImage autorelease];	
}*/
{
	if (angle != 90 && angle != 270)
		return self;
	
    NSImage *existingImage = self;
    NSSize existingSize;
	
    /**
     * Get the size of the original image in its raw bitmap format.
     * The bestRepresentationForDevice: nil tells the NSImage to just
     * give us the raw image instead of it's wacky DPI-translated version.
     */
    existingSize.width = [[existingImage bestRepresentationForDevice: nil] pixelsWide];
    existingSize.height = [[existingImage bestRepresentationForDevice: nil] pixelsHigh];
	
    NSSize newSize = NSMakeSize(existingSize.height, existingSize.width);
    NSImage *rotatedImage = [[NSImage alloc] initWithSize:newSize];
	
    [rotatedImage lockFocus];
	
    /**
     * Apply the following transformations:
     *
     * - bring the rotation point to the centre of the image instead of
     *   the default lower, left corner (0,0).
     * - rotate it by 90 degrees, either clock or counter clockwise.
     * - re-translate the rotated image back down to the lower left corner
     *   so that it appears in the right place.
     */
    NSAffineTransform *rotateTF = [NSAffineTransform transform];
    NSPoint centerPoint = NSMakePoint(newSize.width / 2, newSize.height / 2);
	
    [rotateTF translateXBy: centerPoint.x yBy: centerPoint.y];
    [rotateTF rotateByDegrees: angle];
    [rotateTF translateXBy: -centerPoint.y yBy: -centerPoint.x];
    [rotateTF concat];
	
    /**
     * We have to get the image representation to do its drawing directly,
     * because otherwise the stupid NSImage DPI thingie bites us in the butt
     * again.
     */
    NSRect r1 = NSMakeRect(0, 0, newSize.height, newSize.width);
    [[existingImage bestRepresentationForDevice: nil] drawInRect: r1];
	
    [rotatedImage unlockFocus];
	
    return rotatedImage;
}

@end
