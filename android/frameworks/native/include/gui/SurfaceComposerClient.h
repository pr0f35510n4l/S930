/*
 * Copyright (C) 2007 The Android Open Source Project
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

#ifndef ANDROID_GUI_SURFACE_COMPOSER_CLIENT_H
#define ANDROID_GUI_SURFACE_COMPOSER_CLIENT_H

#include <stdint.h>
#include <sys/types.h>

#include <binder/IBinder.h>
#include <binder/IMemory.h>

#include <utils/RefBase.h>
#include <utils/Singleton.h>
#include <utils/SortedVector.h>
#include <utils/threads.h>

#include <ui/PixelFormat.h>

#include <gui/CpuConsumer.h>
#include <gui/SurfaceControl.h>

namespace android {

// ---------------------------------------------------------------------------

class DisplayInfo;
class Composer;
class ISurfaceComposerClient;
class IGraphicBufferProducer;
class Region;

#ifndef MTK_DEFAULT_AOSP
class DisplayInfoEx;
#endif

// ---------------------------------------------------------------------------

class SurfaceComposerClient : public RefBase
{
    friend class Composer;
public:
                SurfaceComposerClient();
    virtual     ~SurfaceComposerClient();

    // Always make sure we could initialize
    status_t    initCheck() const;

    // Return the connection of this client
    sp<IBinder> connection() const;

    // Forcibly remove connection before all references have gone away.
    void        dispose();

    // callback when the composer is dies
    status_t linkToComposerDeath(const sp<IBinder::DeathRecipient>& recipient,
            void* cookie = NULL, uint32_t flags = 0);

    // Get information about a display
    static status_t getDisplayInfo(const sp<IBinder>& display, DisplayInfo* info);

    /* triggers screen off and waits for it to complete */
    static void blankDisplay(const sp<IBinder>& display);

    /* triggers screen on and waits for it to complete */
    static void unblankDisplay(const sp<IBinder>& display);

    // ------------------------------------------------------------------------
    // surface creation / destruction

    //! Create a surface
    sp<SurfaceControl> createSurface(
            const String8& name,// name of the surface
            uint32_t w,         // width in pixel
            uint32_t h,         // height in pixel
            PixelFormat format, // pixel-format desired
            uint32_t flags = 0  // usage flags
    );

    //! Create a virtual display
    static sp<IBinder> createDisplay(const String8& displayName, bool secure);

    //! Destroy a virtual display
    static void destroyDisplay(const sp<IBinder>& display);

    //! Get the token for the existing default displays.
    //! Possible values for id are eDisplayIdMain and eDisplayIdHdmi.
    static sp<IBinder> getBuiltInDisplay(int32_t id);

    // ------------------------------------------------------------------------
    // Composer parameters
    // All composer parameters must be changed within a transaction
    // several surfaces can be updated in one transaction, all changes are
    // committed at once when the transaction is closed.
    // closeGlobalTransaction() requires an IPC with the server.

    //! Open a composer transaction on all active SurfaceComposerClients.
    static void openGlobalTransaction();

    //! Close a composer transaction on all active SurfaceComposerClients.
    static void closeGlobalTransaction(bool synchronous = false);

    //! Flag the currently open transaction as an animation transaction.
    static void setAnimationTransaction();

    status_t    hide(const sp<IBinder>& id);
    status_t    show(const sp<IBinder>& id);
    status_t    setFlags(const sp<IBinder>& id, uint32_t flags, uint32_t mask);
    status_t    setTransparentRegionHint(const sp<IBinder>& id, const Region& transparent);
    status_t    setLayer(const sp<IBinder>& id, int32_t layer);
    status_t    setAlpha(const sp<IBinder>& id, float alpha=1.0f);
    status_t    setMatrix(const sp<IBinder>& id, float dsdx, float dtdx, float dsdy, float dtdy);
    status_t    setPosition(const sp<IBinder>& id, float x, float y);
    status_t    setSize(const sp<IBinder>& id, uint32_t w, uint32_t h);
    status_t    setCrop(const sp<IBinder>& id, const Rect& crop);
    status_t    setLayerStack(const sp<IBinder>& id, uint32_t layerStack);
    status_t    destroySurface(const sp<IBinder>& id);

    static void setDisplaySurface(const sp<IBinder>& token,
            const sp<IGraphicBufferProducer>& bufferProducer);
    static void setDisplayLayerStack(const sp<IBinder>& token,
            uint32_t layerStack);

    /* setDisplayProjection() defines the projection of layer stacks
     * to a given display.
     *
     * - orientation defines the display's orientation.
     * - layerStackRect defines which area of the window manager coordinate
     * space will be used.
     * - displayRect defines where on the display will layerStackRect be
     * mapped to. displayRect is specified post-orientation, that is
     * it uses the orientation seen by the end-user.
     */
    static void setDisplayProjection(const sp<IBinder>& token,
            uint32_t orientation,
            const Rect& layerStackRect,
            const Rect& displayRect);

private:
    virtual void onFirstRef();
    Composer& getComposer();

    mutable     Mutex                       mLock;
                status_t                    mStatus;
                sp<ISurfaceComposerClient>  mClient;
                Composer&                   mComposer;

#ifndef MTK_DEFAULT_AOSP
public:
    static status_t getDisplayInfoEx(const sp<IBinder>& display, DisplayInfoEx* info);

    // setting extra surface flags
    status_t    setFlagsEx(const sp<IBinder>& id, uint32_t flags, uint32_t mask);
#endif
};

// ---------------------------------------------------------------------------

class ScreenshotClient
{
public:
    static status_t capture(
            const sp<IBinder>& display,
            const sp<IGraphicBufferProducer>& producer,
            uint32_t reqWidth, uint32_t reqHeight,
            uint32_t minLayerZ, uint32_t maxLayerZ);

private:
    mutable sp<CpuConsumer> mCpuConsumer;
    mutable sp<BufferQueue> mBufferQueue;
    CpuConsumer::LockedBuffer mBuffer;
    bool mHaveBuffer;

public:
    ScreenshotClient();
    ~ScreenshotClient();

    // frees the previous screenshot and capture a new one
    status_t update(const sp<IBinder>& display);
    status_t update(const sp<IBinder>& display,
            uint32_t reqWidth, uint32_t reqHeight);
    status_t update(const sp<IBinder>& display,
            uint32_t reqWidth, uint32_t reqHeight,
            uint32_t minLayerZ, uint32_t maxLayerZ);

/*Lenovo-sw yangyang19 add 20140320 for screenshot begin*/
    status_t update_lenovo(const sp<IBinder>& display);
    status_t update_lenovo(const sp<IBinder>& display,
            uint32_t reqWidth, uint32_t reqHeight);
    status_t update_lenovo(const sp<IBinder>& display,
            uint32_t reqWidth, uint32_t reqHeight,
            uint32_t minLayerZ, uint32_t maxLayerZ);
/*Lenovo-sw yangyang19 add 20140320 for screenshot end*/
    
    sp<CpuConsumer> getCpuConsumer() const;

    // release memory occupied by the screenshot
    void release();

    // pixels are valid until this object is freed or
    // release() or update() is called
    void const* getPixels() const;

    uint32_t getWidth() const;
    uint32_t getHeight() const;
    PixelFormat getFormat() const;
    uint32_t getStride() const;
    // size of allocated memory in bytes
    size_t getSize() const;
};

// ---------------------------------------------------------------------------
}; // namespace android

#endif // ANDROID_GUI_SURFACE_COMPOSER_CLIENT_H
