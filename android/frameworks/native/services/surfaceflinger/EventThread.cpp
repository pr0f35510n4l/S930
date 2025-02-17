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

#define ATRACE_TAG ATRACE_TAG_GRAPHICS

#include <stdint.h>
#include <sys/types.h>

#include <cutils/compiler.h>

#include <gui/BitTube.h>
#include <gui/IDisplayEventConnection.h>
#include <gui/DisplayEventReceiver.h>

#include <utils/Errors.h>
#include <utils/String8.h>
#include <utils/Trace.h>

#include "EventThread.h"
#include "SurfaceFlinger.h"

#ifndef MTK_DEFAULT_AOSP
// reserve client pid infomation
#include <binder/IPCThreadState.h>
#include <cutils/xlog.h>
#endif

//huangjc1 added 2014.04.21 for FFI Debug Info begin
#define FFI_CAPACITY 256
//huangjc1 added 2014.04.21 for FFI Debug Info end

// ---------------------------------------------------------------------------
namespace android {
// ---------------------------------------------------------------------------

EventThread::EventThread(const sp<VSyncSource>& src)
    : mVSyncSource(src),
      mUseSoftwareVSync(false),
      mVsyncEnabled(false),
      mDebugVsyncEnabled(false)
	/* -------------------- Lenovo Edit Begin -------------------- */
	// liubin modified 2013.09.05, reduce FPS
	,VSyncIsNormal( true )
	/* -------------------- Lenovo Edit End -------------------- */
{

    for (int32_t i=0 ; i<DisplayDevice::NUM_BUILTIN_DISPLAY_TYPES ; i++) {
        mVSyncEvent[i].header.type = DisplayEventReceiver::DISPLAY_EVENT_VSYNC;
        mVSyncEvent[i].header.id = 0;
        mVSyncEvent[i].header.timestamp = 0;
        mVSyncEvent[i].vsync.count =  0;
    }
}

//huangjc1 added 2014.04.21 for FFI Debug Info begin
EventThreadEx::EventThreadEx(const sp<VSyncSource>& src)
	:EventThread(src),mOldFrameCount(0),mJankCount(0)
{
    for (int32_t i=0 ; i<DisplayDevice::NUM_BUILTIN_DISPLAY_TYPES ; i++) {
        mVSyncEvent[i].header.type = DisplayEventReceiver::DISPLAY_EVENT_VSYNC;
        mVSyncEvent[i].header.id = 0;
        mVSyncEvent[i].header.timestamp = 0;
        mVSyncEvent[i].vsync.count =  0;
    }
    mFrameRecord.setCapacity(FFI_CAPACITY);
    char value[PROPERTY_VALUE_MAX];
    property_get("debug.sf.ffilog", value, "0");
    mFFILogSwitch = atoi(value);
}
//huangjc1 added 2014.04.21 for FFI Debug Info end

void EventThread::onFirstRef() {
    run("EventThread", PRIORITY_URGENT_DISPLAY + PRIORITY_MORE_FAVORABLE);
}

sp<EventThread::Connection> EventThread::createEventConnection() const {
    return new Connection(const_cast<EventThread*>(this));
}

status_t EventThread::registerDisplayEventConnection(
        const sp<EventThread::Connection>& connection) {
    Mutex::Autolock _l(mLock);
    mDisplayEventConnections.add(connection);
    mCondition.broadcast();
    return NO_ERROR;
}

void EventThread::removeDisplayEventConnection(
        const wp<EventThread::Connection>& connection) {
    Mutex::Autolock _l(mLock);
    mDisplayEventConnections.remove(connection);
}

void EventThread::setVsyncRate(uint32_t count,
        const sp<EventThread::Connection>& connection) {
    if (int32_t(count) >= 0) { // server must protect against bad params
        Mutex::Autolock _l(mLock);
        const int32_t new_count = (count == 0) ? -1 : count;
        if (connection->count != new_count) {
            connection->count = new_count;
            mCondition.broadcast();
        }
    }
}

/* -------------------- Lenovo Edit Begin -------------------- */
// liubin modified 2013.09.05, reduce FPS
void EventThread::setVsyncType( bool isNormal, const sp<EventThread::Connection>& connection )
{
	Mutex::Autolock _l( mLock );
	VSyncIsNormal = isNormal;

	return;
}
/* -------------------- Lenovo Edit End -------------------- */

void EventThread::requestNextVsync(
        const sp<EventThread::Connection>& connection) {
    Mutex::Autolock _l(mLock);
    if (connection->count < 0) {
        connection->count = 0;
        mCondition.broadcast();
    }
}

void EventThread::onScreenReleased() {
    Mutex::Autolock _l(mLock);
    if (!mUseSoftwareVSync) {
        // disable reliance on h/w vsync
        mUseSoftwareVSync = true;
        mCondition.broadcast();
    }
}

void EventThread::onScreenAcquired() {
    Mutex::Autolock _l(mLock);
    if (mUseSoftwareVSync) {
        // resume use of h/w vsync
        mUseSoftwareVSync = false;
        mCondition.broadcast();
    }
}

void EventThread::onVSyncEvent(nsecs_t timestamp) {
    Mutex::Autolock _l(mLock);
	/* -------------------- Lenovo Edit Begin -------------------- */
	// liubin modified 2013.09.05, reduce FPS
	if ( !VSyncIsNormal )
	{
		//VSyncIsNormal = true;

		mVSyncEvent[0].vsync.count++;

		if ( ( ( mVSyncEvent[ 0 ].vsync.count % 3 ) == 0 )
			|| ( ( mVSyncEvent[ 0 ].vsync.count % 3 ) == 1 ) )
		{
			mVSyncEvent[0].header.type = DisplayEventReceiver::DISPLAY_EVENT_VSYNC;
			mVSyncEvent[0].header.id = 0;
			mVSyncEvent[0].header.timestamp = timestamp;

			mCondition.broadcast();
		}
	}
	else
	/* -------------------- Lenovo Edit End -------------------- */
	{
		mVSyncEvent[0].header.type = DisplayEventReceiver::DISPLAY_EVENT_VSYNC;
		mVSyncEvent[0].header.id = 0;
		mVSyncEvent[0].header.timestamp = timestamp;
		mVSyncEvent[0].vsync.count++;
		mCondition.broadcast();
	}
}

//huangjc1 added 2014.04.21 for FFI Debug Info begin
void EventThreadEx::onVSyncEvent(nsecs_t timestamp) {
    Mutex::Autolock _l(mLock);

    if (VSyncIsNormal) {
        //ALOGE("fps onVSyncEvent VSyncIsNormal");
	mVSyncEvent[0].header.type = DisplayEventReceiver::DISPLAY_EVENT_VSYNC;
	mVSyncEvent[0].header.id = 0;
	mVSyncEvent[0].header.timestamp = timestamp;
	mVSyncEvent[0].vsync.count++;
      uint32_t newFrameCount = DisplayDevice::getPageFlipCountGlobal();
      FrameRecord fr;

      if(newFrameCount == mOldFrameCount)
      {
        mJankCount++;
      }else if(newFrameCount - mOldFrameCount == 1) {
        fr.timestamp = systemTime();
        fr.frameCount= newFrameCount;
        fr.jankCount = mJankCount;
        mJankCount = 0;
        if(mFrameRecord.empty())
        {
          fr.interval = ms2ns(16);
        }else{
          fr.interval = fr.timestamp - mFrameRecord[mFrameRecord.size() - 1].timestamp;
        }
        mFrameRecord.add(fr);
        ALOGE_IF(mFFILogSwitch,"CALC_FFI timediff = %lf jankCount = %d",(double)(fr.interval)/1000000,fr.jankCount);
				if(fr.jankCount > 0)
				{
					ALOGD("ffi_3d_jank timespan = %lf jankCount = %d",fr.interval*0.000001f,fr.jankCount);
				}

				//no frame be flip more than 100ms
        if(((fr.interval- (nsecs_t)(100*1000000)) > 0) && (fr.jankCount == 0))
        {
          ALOGE("CALC_FFI long time interval between 2 coherent frame! interval = %lf ",(double)(fr.interval)/1000000);
        }
      }else{
        ALOGE("SOMETHING WRONG HERE !!! newFrameCount - oldFrameCount = %d",newFrameCount - mOldFrameCount);
      }

      mOldFrameCount = newFrameCount;

      if(mFrameRecord.size() >= FFI_CAPACITY)
      {
        ALOGE_IF(mFFILogSwitch,"CALC_FFI\tinterval\t\ttimestamp\t\tjankCount\t\t");
        for(int i = 0;i<FFI_CAPACITY;i++)
        {
          ALOGE_IF(mFFILogSwitch,"CALC_FFI\t%10.6lf\t\t%lld\t\t%d",((double)mFrameRecord[i].interval)/1000000,mFrameRecord[i].timestamp,mFrameRecord[i].jankCount);
        }
        FrameRecord tmpRecord = mFrameRecord[mFrameRecord.size() - 1];
        mFrameRecord.clear();
        mFrameRecord.add(tmpRecord);
      }
	mCondition.broadcast();
    }
	else { // !VSyncIsNormal
        //ALOGE("fps onVSyncEvent VSyncIsABNormal");
        mVSyncEvent[0].vsync.count++;
        if ((mVSyncEvent[0].vsync.count % 2) == 0)
        {
            mVSyncEvent[0].header.type = DisplayEventReceiver::DISPLAY_EVENT_VSYNC;
            mVSyncEvent[0].header.id = 0;
            mVSyncEvent[0].header.timestamp = timestamp;
            mCondition.broadcast();
        }
    }
}
//huangjc1 added 2014.04.21 for FFI Debug Info end

void EventThread::onHotplugReceived(int type, bool connected) {
    ALOGE_IF(type >= DisplayDevice::NUM_BUILTIN_DISPLAY_TYPES,
            "received hotplug event for an invalid display (id=%d)", type);

    Mutex::Autolock _l(mLock);
    if (type < DisplayDevice::NUM_BUILTIN_DISPLAY_TYPES) {
        DisplayEventReceiver::Event event;
        event.header.type = DisplayEventReceiver::DISPLAY_EVENT_HOTPLUG;
        event.header.id = type;
        event.header.timestamp = systemTime();
        event.hotplug.connected = connected;
        mPendingEvents.add(event);
        mCondition.broadcast();
    }
}

bool EventThread::threadLoop() {
    DisplayEventReceiver::Event event;
    Vector< sp<EventThread::Connection> > signalConnections;
    signalConnections = waitForEvent(&event);

    // dispatch events to listeners...
    const size_t count = signalConnections.size();
    for (size_t i=0 ; i<count ; i++) {
        const sp<Connection>& conn(signalConnections[i]);
        // now see if we still need to report this event
        status_t err = conn->postEvent(event);
        if (err == -EAGAIN || err == -EWOULDBLOCK) {
            // The destination doesn't accept events anymore, it's probably
            // full. For now, we just drop the events on the floor.
            // FIXME: Note that some events cannot be dropped and would have
            // to be re-sent later.
            // Right-now we don't have the ability to do this.
            ALOGW("EventThread: dropping event (%08x) for connection %p",
                    event.header.type, conn.get());
        } else if (err < 0) {
            // handle any other error on the pipe as fatal. the only
            // reasonable thing to do is to clean-up this connection.
            // The most common error we'll get here is -EPIPE.
            removeDisplayEventConnection(signalConnections[i]);
        }
    }
    return true;
}

// This will return when (1) a vsync event has been received, and (2) there was
// at least one connection interested in receiving it when we started waiting.
Vector< sp<EventThread::Connection> > EventThread::waitForEvent(
        DisplayEventReceiver::Event* event)
{
    Mutex::Autolock _l(mLock);
    Vector< sp<EventThread::Connection> > signalConnections;

    do {
        bool eventPending = false;
        bool waitForVSync = false;

        size_t vsyncCount = 0;
        nsecs_t timestamp = 0;
        for (int32_t i=0 ; i<DisplayDevice::NUM_BUILTIN_DISPLAY_TYPES ; i++) {
            timestamp = mVSyncEvent[i].header.timestamp;
            if (timestamp) {
                // we have a vsync event to dispatch
                *event = mVSyncEvent[i];
                mVSyncEvent[i].header.timestamp = 0;
                vsyncCount = mVSyncEvent[i].vsync.count;
                break;
            }
        }

        if (!timestamp) {
            // no vsync event, see if there are some other event
            eventPending = !mPendingEvents.isEmpty();
            if (eventPending) {
                // we have some other event to dispatch
                *event = mPendingEvents[0];
                mPendingEvents.removeAt(0);
            }
        }

        // find out connections waiting for events
        size_t count = mDisplayEventConnections.size();
        for (size_t i=0 ; i<count ; i++) {
            sp<Connection> connection(mDisplayEventConnections[i].promote());
            if (connection != NULL) {
                bool added = false;
                if (connection->count >= 0) {
                    // we need vsync events because at least
                    // one connection is waiting for it
                    waitForVSync = true;
                    if (timestamp) {
                        // we consume the event only if it's time
                        // (ie: we received a vsync event)
                        if (connection->count == 0) {
                            // fired this time around
                            connection->count = -1;
                            signalConnections.add(connection);
                            added = true;
                        } else if (connection->count == 1 ||
                                (vsyncCount % connection->count) == 0) {
                            // continuous event, and time to report it
                            signalConnections.add(connection);
                            added = true;
                        }
                    }
                }

                if (eventPending && !timestamp && !added) {
                    // we don't have a vsync event to process
                    // (timestamp==0), but we have some pending
                    // messages.
                    signalConnections.add(connection);
                }
            } else {
                // we couldn't promote this reference, the connection has
                // died, so clean-up!
                mDisplayEventConnections.removeAt(i);
                --i; --count;
            }
        }

        // Here we figure out if we need to enable or disable vsyncs
        if (timestamp && !waitForVSync) {
            // we received a VSYNC but we have no clients
            // don't report it, and disable VSYNC events
            disableVSyncLocked();
        } else if (!timestamp && waitForVSync) {
            // we have at least one client, so we want vsync enabled
            // (TODO: this function is called right after we finish
            // notifying clients of a vsync, so this call will be made
            // at the vsync rate, e.g. 60fps.  If we can accurately
            // track the current state we could avoid making this call
            // so often.)
            enableVSyncLocked();
        }

        // note: !timestamp implies signalConnections.isEmpty(), because we
        // don't populate signalConnections if there's no vsync pending
        if (!timestamp && !eventPending) {
            // wait for something to happen
            if (waitForVSync) {
                // This is where we spend most of our time, waiting
                // for vsync events and new client registrations.
                //
                // If the screen is off, we can't use h/w vsync, so we
                // use a 16ms timeout instead.  It doesn't need to be
                // precise, we just need to keep feeding our clients.
                //
                // We don't want to stall if there's a driver bug, so we
                // use a (long) timeout when waiting for h/w vsync, and
                // generate fake events when necessary.
                bool softwareSync = mUseSoftwareVSync;
                nsecs_t timeout = softwareSync ? ms2ns(16) : ms2ns(1000);
                if (mCondition.waitRelative(mLock, timeout) == TIMED_OUT) {
                    if (!softwareSync) {
                        ALOGW("Timed out waiting for hw vsync; faking it");
                    }
                    // FIXME: how do we decide which display id the fake
                    // vsync came from ?
                    mVSyncEvent[0].header.type = DisplayEventReceiver::DISPLAY_EVENT_VSYNC;
                    mVSyncEvent[0].header.id = DisplayDevice::DISPLAY_PRIMARY;
                    mVSyncEvent[0].header.timestamp = systemTime(SYSTEM_TIME_MONOTONIC);
                    mVSyncEvent[0].vsync.count++;
                }
            } else {
                // Nobody is interested in vsync, so we just want to sleep.
                // h/w vsync should be disabled, so this will wait until we
                // get a new connection, or an existing connection becomes
                // interested in receiving vsync again.
                mCondition.wait(mLock);
            }
        }
    } while (signalConnections.isEmpty());

    // here we're guaranteed to have a timestamp and some connections to signal
    // (The connections might have dropped out of mDisplayEventConnections
    // while we were asleep, but we'll still have strong references to them.)
    return signalConnections;
}

void EventThread::enableVSyncLocked() {
    if (!mUseSoftwareVSync) {
        // never enable h/w VSYNC when screen is off
        if (!mVsyncEnabled) {
            mVsyncEnabled = true;
            mVSyncSource->setCallback(static_cast<VSyncSource::Callback*>(this));
            mVSyncSource->setVSyncEnabled(true);
            mPowerHAL.vsyncHint(true);
        }
    }
    mDebugVsyncEnabled = true;
}

void EventThread::disableVSyncLocked() {
    if (mVsyncEnabled) {
        mVsyncEnabled = false;
        mVSyncSource->setVSyncEnabled(false);
        mPowerHAL.vsyncHint(false);
        mDebugVsyncEnabled = false;
    }
}

void EventThread::dump(String8& result) const {
    Mutex::Autolock _l(mLock);
    result.appendFormat("VSYNC state: %s\n",
            mDebugVsyncEnabled?"enabled":"disabled");
    result.appendFormat("  soft-vsync: %s\n",
            mUseSoftwareVSync?"enabled":"disabled");
    result.appendFormat("  numListeners=%u,\n  events-delivered: %u\n",
            mDisplayEventConnections.size(),
            mVSyncEvent[DisplayDevice::DISPLAY_PRIMARY].vsync.count);
    for (size_t i=0 ; i<mDisplayEventConnections.size() ; i++) {
        sp<Connection> connection =
                mDisplayEventConnections.itemAt(i).promote();
        result.appendFormat("    %p: count=%d\n",
                connection.get(), connection!=NULL ? connection->count : 0);
    }
}

// ---------------------------------------------------------------------------

EventThread::Connection::Connection(
        const sp<EventThread>& eventThread)
    : count(-1), mEventThread(eventThread), mChannel(new BitTube())
{
#ifndef MTK_DEFAULT_AOSP
    // reserve client pid infomation
    pid = IPCThreadState::self()->getCallingPid();
    XLOGI("EventThread Client Pid (%d) created", pid);
#endif
}

EventThread::Connection::~Connection() {
    // do nothing here -- clean-up will happen automatically
    // when the main thread wakes up

#ifndef MTK_DEFAULT_AOSP
    // reserve client pid infomation
    XLOGI("EventThread Client Pid (%d) disconnected by (%d)", pid, getpid());
#endif
}

void EventThread::Connection::onFirstRef() {
    // NOTE: mEventThread doesn't hold a strong reference on us
    mEventThread->registerDisplayEventConnection(this);
	/* -------------------- Lenovo Edit Begin -------------------- */
	// liubin modified 2013.09.05, reduce FPS
	setVsyncType( true );
	/* -------------------- Lenovo Edit End -------------------- */
}

sp<BitTube> EventThread::Connection::getDataChannel() const {
    return mChannel;
}

void EventThread::Connection::setVsyncRate(uint32_t count) {
#ifndef MTK_DEFAULT_AOSP
#ifndef MTK_USER_BUILD
    XLOGD("setVsyncRate(%d, c=%d)", pid, count);
#endif
#endif
    mEventThread->setVsyncRate(count, this);
}

/* -------------------- Lenovo Edit Begin -------------------- */
// liubin modified 2013.09.05, reduce FPS
void EventThread::Connection::setVsyncType( bool isNormal )
{
	mEventThread->setVsyncType( isNormal, this );

	return;
}
/* -------------------- Lenovo Edit End -------------------- */

void EventThread::Connection::requestNextVsync() {
#ifndef MTK_DEFAULT_AOSP
#ifndef MTK_USER_BUILD
    XLOGD("requestNextVsync(%d)", pid);
#endif
#endif
    mEventThread->requestNextVsync(this);
}

status_t EventThread::Connection::postEvent(
        const DisplayEventReceiver::Event& event) {
#ifndef MTK_DEFAULT_AOSP
#ifndef MTK_USER_BUILD
    if (event.header.type == DisplayEventReceiver::DISPLAY_EVENT_VSYNC)
        XLOGD("postEvent(%d, v/c=%d)", pid, event.vsync.count);
    else
        XLOGD("postEvent(%d)", pid);
#endif
#endif
    ssize_t size = DisplayEventReceiver::sendEvents(mChannel, &event, 1);
    return size < 0 ? status_t(size) : status_t(NO_ERROR);
}

// ---------------------------------------------------------------------------

}; // namespace android
