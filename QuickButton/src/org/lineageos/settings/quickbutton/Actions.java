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

/**
 * Setting names and values shared by the settings screen (runs in this app)
 * and KeyHandler (runs inside system_server). Values live in Settings.Secure
 * so system_server can read them without this app running.
 */
final class Actions {

    /** Action for a short press / a long press. */
    static final String KEY_PRESS = "quickbutton_press_action";
    static final String KEY_LONG_PRESS = "quickbutton_long_press_action";

    /** Action values; anything else is a flattened ComponentName to launch. */
    static final String NONE = "none";
    static final String CAMERA = "camera";

    static String defaultAction(String key) {
        return KEY_PRESS.equals(key) ? CAMERA : NONE;
    }

    private Actions() {
    }
}
