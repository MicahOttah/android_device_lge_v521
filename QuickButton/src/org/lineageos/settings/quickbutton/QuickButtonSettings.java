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

import android.app.Activity;
import android.content.ComponentName;
import android.content.ContentResolver;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.os.Bundle;
import android.preference.ListPreference;
import android.preference.Preference;
import android.preference.PreferenceFragment;
import android.provider.Settings;
import android.view.MenuItem;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public class QuickButtonSettings extends Activity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (getActionBar() != null) {
            getActionBar().setDisplayHomeAsUpEnabled(true);
        }
        if (savedInstanceState == null) {
            getFragmentManager().beginTransaction()
                    .replace(android.R.id.content, new SettingsFragment())
                    .commit();
        }
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == android.R.id.home) {
            finish();
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    public static class SettingsFragment extends PreferenceFragment
            implements Preference.OnPreferenceChangeListener {

        private final List<CharSequence> mEntries = new ArrayList<>();
        private final List<CharSequence> mValues = new ArrayList<>();

        @Override
        public void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            addPreferencesFromResource(R.xml.quickbutton);
            findPreference(Actions.KEY_PRESS).setOnPreferenceChangeListener(this);
            findPreference(Actions.KEY_LONG_PRESS).setOnPreferenceChangeListener(this);
        }

        @Override
        public void onResume() {
            super.onResume();
            // Reload every time: apps may have been installed or removed meanwhile
            loadChoices();
            bindAction(Actions.KEY_PRESS);
            bindAction(Actions.KEY_LONG_PRESS);
        }

        @Override
        public boolean onPreferenceChange(Preference preference, Object newValue) {
            Settings.Secure.putString(getResolver(), preference.getKey(), (String) newValue);
            preference.setSummary(labelFor((String) newValue));
            return true;
        }

        private ContentResolver getResolver() {
            return getActivity().getContentResolver();
        }

        /** Fills the choice list: nothing, camera, then every launcher app by name. */
        private void loadChoices() {
            mEntries.clear();
            mValues.clear();
            mEntries.add(getString(R.string.action_none));
            mValues.add(Actions.NONE);
            mEntries.add(getString(R.string.action_camera));
            mValues.add(Actions.CAMERA);

            final PackageManager pm = getActivity().getPackageManager();
            final List<ResolveInfo> apps = pm.queryIntentActivities(
                    new Intent(Intent.ACTION_MAIN).addCategory(Intent.CATEGORY_LAUNCHER), 0);
            Collections.sort(apps, new ResolveInfo.DisplayNameComparator(pm));
            for (ResolveInfo app : apps) {
                mEntries.add(app.loadLabel(pm));
                mValues.add(new ComponentName(app.activityInfo.packageName,
                        app.activityInfo.name).flattenToString());
            }
        }

        private void bindAction(String key) {
            String value = Settings.Secure.getString(getResolver(), key);
            if (value == null) {
                value = Actions.defaultAction(key);
            }
            final ListPreference pref = (ListPreference) findPreference(key);
            pref.setEntries(mEntries.toArray(new CharSequence[mEntries.size()]));
            pref.setEntryValues(mValues.toArray(new CharSequence[mValues.size()]));
            pref.setValue(value);
            pref.setSummary(labelFor(value));
        }

        private CharSequence labelFor(String value) {
            final int index = mValues.indexOf(value);
            return index >= 0 ? mEntries.get(index) : getString(R.string.action_missing);
        }
    }
}
