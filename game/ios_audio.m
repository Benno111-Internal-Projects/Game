#include "ios_audio.h"
#include "raylib.h"
#import <AVFoundation/AVFoundation.h>

void ConfigureIosGameAudio(void)
{
    @autoreleasepool {
        AVAudioSession *session = [AVAudioSession sharedInstance];
        NSError *error = nil;
        // Raylib's backend starts in PlayAndRecord by default. This game only
        // plays audio, so use media routing without overriding connected outputs.
        if (![session setCategory:AVAudioSessionCategoryPlayback
                              mode:AVAudioSessionModeDefault
                           options:0
                             error:&error])
        {
            TraceLog(LOG_WARNING, "AUDIO: Could not select iOS media routing: %s",
                     [[error localizedDescription] UTF8String]);
        }
    }
}
