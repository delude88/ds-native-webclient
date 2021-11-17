//
// Created by Tobias Hegemann on 19.10.21.
//
#import "macos_internal.h"

@implementation MacOSUtils

+ (bool)check_access_internal {
  try {
    switch ([AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio]) {
      case AVAuthorizationStatusAuthorized: {
        // The user has previously granted access to the camera.
        return true;
      }
      case AVAuthorizationStatusNotDetermined: {
        // The app hasn't yet asked the user for camera access.
        __block BOOL is_granted = false;
        dispatch_semaphore_t sema = dispatch_semaphore_create(0);
        [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio completionHandler:^(BOOL granted) {
          is_granted = granted;
          dispatch_semaphore_signal(sema);
        }];
        dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
        dispatch_release(sema);
        return is_granted;
      }
      case AVAuthorizationStatusDenied: {
        // The user has previously denied access.
        return false;
      }
      case AVAuthorizationStatusRestricted: {
        // The user can't grant access due to restrictions.
        return false;
      }
    }
    return false;
  } catch (...) {
    return false;
  }
}

@end

bool check_access() {
  // Call the Objective-C method using Objective-C syntax
  return [MacOSUtils check_access_internal];
}
