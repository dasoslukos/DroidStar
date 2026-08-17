#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>

#include "ios_audio_session.h"

bool configure_ios_audio_session(void)
{
    @autoreleasepool {
        AVAudioSession *session = [AVAudioSession sharedInstance];
        NSError *error = nil;

        AVAudioSessionCategoryOptions options =
            AVAudioSessionCategoryOptionDefaultToSpeaker |
            AVAudioSessionCategoryOptionAllowBluetooth;

        BOOL ok = [session setCategory:AVAudioSessionCategoryPlayAndRecord
                                  mode:AVAudioSessionModeDefault
                               options:options
                                 error:&error];

        if (!ok) {
            NSLog(@"DroidStar: AVAudioSession setCategory failed: %@", error);
            return false;
        }

        error = nil;

        ok = [session setActive:YES error:&error];

        if (!ok) {
            NSLog(@"DroidStar: AVAudioSession setActive failed: %@", error);
            return false;
        }

        NSLog(@"DroidStar: iOS background audio session active");
        return true;
    }
}
