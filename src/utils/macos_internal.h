#import <objc/NSObject.h>
#import <CoreFoundation/CoreFoundation.h>
#import <AVFoundation/AVFoundation.h>
#import "macos.h"

@interface MacOSUtils : NSObject {
  //AVCaptureDevice *device;
}

+ (bool)check_access_internal;

@end