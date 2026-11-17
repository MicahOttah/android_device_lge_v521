/*
 * Copyright (C) 2026 The LineageOS Project
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

package org.lineageos.settings.quickbutton;

import android.app.KeyguardManager;
import android.content.ActivityNotFoundException;
import android.content.ComponentName;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.PowerManager;
import android.os.RemoteException;
import android.os.UserHandle;
import android.provider.MediaStore;
import android.provider.Settings;
import android.util.Log;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.ViewConfiguration;
import android.view.WindowManagerGlobal;

import com.android.internal.os.DeviceKeyHandler;

/**
 * Handles the V521 side "QuickButton" (gpio-keys hot_key, scancode 377).
 *
 * Loaded into system_server by PhoneWindowManager through
 * config_deviceKeyHandlerLibs/Classes, so the Context here is the system
 * context: this app's resources are not available in this class.
 */
public class KeyHandler implements DeviceKeyHandler {

    private static final String TAG = "QuickButton";

    private static final int SCANCODE_HOT_KEY = 377;
    private static final String INPUT_DEVICE_NAME = "gpio-keys";

    private static final int MSG_PRESS = 1;
    private static final int MSG_LONG_PRESS = 2;

    private final Context mContext;
    private final PowerManager mPowerManager;
    private final Handler mHandler;
    private final int mLongPressTimeout;

    private final Object mLock = new Object();
    // State of the press in progress, guarded by mLock
    private boolean mTracking;
    private boolean mLongPressFired;
    private String mPressAction;
    private String mLongPressAction;

    public KeyHandler(Context context) {
        mContext = context;
        mPowerManager = (PowerManager) context.getSystemService(Context.POWER_SERVICE);
        mLongPressTimeout = ViewConfiguration.getLongPressTimeout();
        mHandler = new Handler(Looper.getMainLooper()) {
            @Override
            public void handleMessage(Message msg) {
                String action;
                synchronized (mLock) {
                    if (msg.what == MSG_LONG_PRESS) {
                        if (!mTracking) {
                            return; // released just before the timeout
                        }
                        mLongPressFired = true;
                        action = mLongPressAction;
                    } else {
                        action = mPressAction;
                    }
                }
                launch(action);
            }
        };
    }

    /**
     * Called on the input reader thread for every key.
     * Returning null consumes the event.
     */
    @Override
    public KeyEvent handleKeyEvent(KeyEvent event) {
        if (event.getScanCode() != SCANCODE_HOT_KEY || !isFromGpioKeys(event)) {
            return event;
        }

        if (event.getAction() == KeyEvent.ACTION_DOWN) {
            if (event.getRepeatCount() != 0) {
                synchronized (mLock) {
                    return mTracking ? null : event;
                }
            }
            return onDown() ? null : event;
        }

        synchronized (mLock) {
            if (!mTracking) {
                return event;
            }
            mTracking = false;
            mHandler.removeMessages(MSG_LONG_PRESS);
            if (!mLongPressFired && !event.isCanceled()) {
                mHandler.sendEmptyMessage(MSG_PRESS);
            }
        }
        return null;
    }

    /** @return true if this press is ours to handle */
    private boolean onDown() {
        // The button is deliberately ignored while the screen is off
        if (!mPowerManager.isInteractive()) {
            return false;
        }
        final ContentResolver cr = mContext.getContentResolver();
        final int user = UserHandle.USER_CURRENT;

        if (Settings.Secure.getIntForUser(cr, Settings.Secure.USER_SETUP_COMPLETE, 0, user) == 0) {
            return false;
        }
        final String press = getAction(cr, Actions.KEY_PRESS);
        final String longPress = getAction(cr, Actions.KEY_LONG_PRESS);
        final boolean hasLongPress = !Actions.NONE.equals(longPress);
        if (Actions.NONE.equals(press) && !hasLongPress) {
            return false;
        }
        synchronized (mLock) {
            mTracking = true;
            mLongPressFired = false;
            mPressAction = press;
            mLongPressAction = longPress;
            mHandler.removeMessages(MSG_LONG_PRESS);
            if (hasLongPress) {
                mHandler.sendEmptyMessageDelayed(MSG_LONG_PRESS, mLongPressTimeout);
            }
        }
        return true;
    }

    private static String getAction(ContentResolver cr, String key) {
        final String value = Settings.Secure.getStringForUser(cr, key, UserHandle.USER_CURRENT);
        return value != null ? value : Actions.defaultAction(key);
    }

    private static boolean isFromGpioKeys(KeyEvent event) {
        final InputDevice device = event.getDevice();
        return device != null && INPUT_DEVICE_NAME.equals(device.getName());
    }

    private void launch(String action) {
        if (action == null || Actions.NONE.equals(action)) {
            return;
        }

        final KeyguardManager km =
                (KeyguardManager) mContext.getSystemService(Context.KEYGUARD_SERVICE);
        final boolean locked = km != null && km.isKeyguardLocked();

        final Intent intent;
        boolean showsOverKeyguard = false;
        if (Actions.CAMERA.equals(action)) {
            // The secure variant opens on top of the lock screen
            intent = new Intent(locked
                    ? MediaStore.INTENT_ACTION_STILL_IMAGE_CAMERA_SECURE
                    : MediaStore.INTENT_ACTION_STILL_IMAGE_CAMERA);
            showsOverKeyguard = locked;
        } else {
            final ComponentName component = ComponentName.unflattenFromString(action);
            if (component == null) {
                Log.w(TAG, "Ignoring malformed action " + action);
                return;
            }
            intent = new Intent(Intent.ACTION_MAIN)
                    .addCategory(Intent.CATEGORY_LAUNCHER)
                    .setComponent(component);
        }
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK
                | Intent.FLAG_ACTIVITY_RESET_TASK_IF_NEEDED);

        try {
            mContext.startActivityAsUser(intent, null, UserHandle.CURRENT);
        } catch (ActivityNotFoundException | SecurityException e) {
            Log.w(TAG, "Could not start " + intent, e);
            return;
        }
        if (locked && !showsOverKeyguard) {
            // Ask for unlock; the app is waiting behind the lock screen
            try {
                // Android 9 added the message of the unlock prompt to
                // dismissKeyguard(IKeyguardDismissCallback, CharSequence).
                WindowManagerGlobal.getWindowManagerService().dismissKeyguard(null, null);
            } catch (RemoteException e) {
                // system_server is going down
            }
        }
    }
}
