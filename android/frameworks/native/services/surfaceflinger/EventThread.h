/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_SURFACE_FLINGER_EVENT_THREAD_H
#define ANDROID_SURFACE_FLINGER_EVENT_THREAD_H

#include <stdint.h>
#include <sys/types.h>

#include <gui/DisplayEventReceiver.h>
#include <gui/IDisplayEventConnection.h>

#include <utils/Errors.h>
#include <utils/threads.h>
#include <utils/SortedVector.h>

#include "DisplayDevice.h"
#include "DisplayHardware/PowerHAL.h"

// ---------------------------------------------------------------------------
namespace android {
// ---------------------------------------------------------------------------

class SurfaceFlinger;
class String8;

// ---------------------------------------------------------------------------


class VSyncSource : public virtual RefBase {
public:
    class Callback: public virtual RefBase {
    public:
        virtual ~Callback() {}
        virtual void onVSyncEvent(nsecs_t when) = 0;
    };

    virtual ~VSyncSource() {}
    virtual void setVSyncEnabled(bool enable) = 0;
    virtual void setCallback(const sp<Callback>& callback) = 0;
};

class EventThread : public Thread, private VSyncSource::Callback {
    class Connection : public BnDisplayEventConnection {
    public:
        Connection(const sp<EventThread>& eventThread);
        status_t postEvent(const DisplayEventReceiver::Event& event);

        // count >= 1 : continuous event. count is the vsync rate
        // count == 0 : one-shot event that has not fired
        // count ==-1 : one-shot event that fired this round / disabled
        int32_t count;

#ifndef MTK_DEFAULT_AOSP
        // reserve client pid infomation
        int32_t pid;
#endif
		/* -------------------- Lenovo Edit Begin -------------------- */
		// liubin modified 2013.09.05, reduce FPS
		// for multi VSync rate
		bool VSyncIsNormal;
		/* -------------------- Lenovo Edit End -------------------- */

    private:
        virtual ~Connection();
        virtual void onFirstRef();
        virtual sp<BitTube> getDataChannel() const;
        virtual void setVsyncRate(uint32_t count);
        virtual void requestNextVsync();    // asynchronous
        sp<EventThread> const mEventThread;
        sp<BitTube> const mChannel;
		/* -------------------- Lenovo Edit Begin -------------------- */
		// liubin modified 2013.09.05, reduce FPS
		virtual void setVsyncType( bool isNormal );
		/* -------------------- Lenovo Edit End -------------------- */
    };

public:

    EventThread(const sp<VSyncSource>& src);

    sp<Connection> createEventConnection() const;
    status_t registerDisplayEventConnection(const sp<Connection>& connection);

    void setVsyncRate(uint32_t count, const sp<Connection>& connection);
	/* -------------------- Lenovo Edit Begin -------------------- */
	// liubin modified 2013.09.05, reduce FPS
	void setVsyncType( bool isNormal, const sp<Connection>& connection );
	/* -------------------- Lenovo Edit End -------------------- */
    void requestNextVsync(const sp<Connection>& connection);

    // called before the screen is turned off from main thread
    void onScreenReleased();

    // called after the screen is turned on from main thread
    void onScreenAcquired();

    // called when receiving a hotplug event
    void onHotplugReceived(int type, bool connected);

    Vector< sp<EventThread::Connection> > waitForEvent(
            DisplayEventReceiver::Event* event);

    void dump(String8& result) const;

//huangjc1 modified 2014.04.21 for FFI Debug Info begin
//private:
protected:
//huangjc1 modified 2014.04.21 for FFI Debug Info end
    virtual bool        threadLoop();
    virtual void        onFirstRef();

    virtual void onVSyncEvent(nsecs_t timestamp);

    void removeDisplayEventConnection(const wp<Connection>& connection);
    void enableVSyncLocked();
    void disableVSyncLocked();

    // constants
    sp<VSyncSource> mVSyncSource;
    PowerHAL mPowerHAL;

    mutable Mutex mLock;
    mutable Condition mCondition;

    // protected by mLock
    SortedVector< wp<Connection> > mDisplayEventConnections;
    Vector< DisplayEventReceiver::Event > mPendingEvents;
    DisplayEventReceiver::Event mVSyncEvent[DisplayDevice::NUM_BUILTIN_DISPLAY_TYPES];
    bool mUseSoftwareVSync;
    bool mVsyncEnabled;

    // for debugging
    bool mDebugVsyncEnabled;

	/* -------------------- Lenovo Edit Begin -------------------- */
	// liubin modified 2013.09.05, reduce FPS
	// for multi VSync rate
	bool VSyncIsNormal;
	/* -------------------- Lenovo Edit End -------------------- */
};

// ---------------------------------------------------------------------------

//huangjc1 added 2014.04.21 for FFI Debug Info begin
//class EventThreadEx : public EventThread, private VSyncSource::Callback {
class EventThreadEx : virtual public EventThread{
	public:
     EventThreadEx(const sp<VSyncSource>& src);
	private:
    virtual void onVSyncEvent(nsecs_t timestamp);
		struct FrameRecord{
			nsecs_t timestamp;
			nsecs_t interval;
			uint32_t frameCount;
			uint32_t jankCount;
		};
	  Vector<FrameRecord> mFrameRecord;
		uint32_t mOldFrameCount;
		nsecs_t mLastTimeStamp;
		uint32_t mJankCount;
		bool mFFILogSwitch;
};
//huangjc1 added 2014.04.21 for FFI Debug Info end
}; // namespace android

// ---------------------------------------------------------------------------

#endif /* ANDROID_SURFACE_FLINGER_EVENT_THREAD_H */
